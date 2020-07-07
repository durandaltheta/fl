#ifndef __CPP_FUNCTIONAL_LISPLIKE__
#define __CPP_FUNCTIONAL_LISPLIKE__

// c++
#include <memory>
#include <any>
#include <typeinfo>
#include <type_traits>
#include <iostream>
#include <ostream>
#include <mutex>
#include <condition_variable>
#include <string>
#include <stringstream>
#include <unordered_map>
#include <vector>
#include <list>
#include <exception>
#include <thread>

namespace fl { 

//-----------------------------------------------------------------------------
// generic templates
namespace detail {
template <typename T>
using unqualified = typename std::decay<T>::value;

template <typename R, typename... As>
struct function_traits<R(As...)>
{
    typedef R return_type;
    constexpr size_t arity = sizeof...(As);
    template <size_t i>
    using arg = typename std::tuple_element<i, std::tuple<As...>>::type;
};
}



//-----------------------------------------------------------------------------
// fl::function

class atom; // forward declaration

// fl::function definition, an std::function that takes 1 atom as an 
// argument and returns an atom
typedef std::function<atom(atom)> function;

template <typename R, typename... As>
const std::function<R(As...)>& to_std_function(const std::function<R(As...)>& f){ return f; }

template <typename R, typename... As>
std::function<R(As...)> to_std_function(std::function<R(As...)>&& f){ return std::move(f); }

template <typename F>
std::function<decltype(F)> to_std_function(F&& f)
{ 
    std::function<decltype(F)> std_f = f;
    return std_f;
}

// curry 
// transform any std::function into one that accepts its arguments as atoms one 
// at a time
namespace detail {
template <typename FUNCTION> struct
curry;

// final specialization
template <> struct
curry<function> 
{
    using
    type = function;

    const type
    result;

    curry(type fun) : result(fun) {}
};

// specialization for void return type
template <> struct
curry<std::function<void(atom)>> 
{
    using
    type = std::function<R(T)>;

    const type
    result;

    curry(std::function<R(atom)> fun) : 
    result(
        [=](atom a){
            return curry<function>(
                [=](){
                    fun(a);
                    return atom(); // return nil
                }
            ).result;
        }
    ) 
    {}
};

// specialization for no argument and no return value
template <> struct
curry<std::function<void()>> 
{
    using
    type = std::function<void()>;

    const type
    result;

    curry(std::function<void()> fun) : 
    result(
        [=](atom a){
            return curry<function>(
                [=](){
                    fun();
                    return atom();
                }
            ).result;
        }
    ) 
    {}
};

// specialization for no argument
template <typename R> struct
curry<std::function<R()>> 
{
    using
    type = std::function<R()>;

    const type
    result;

    curry(std::function<R()> fun) : 
    result(
        [=](atom a){
            return curry<function>(
                [=](){
                    return atom(fun());
                }
            ).result;
        }
    ) 
    {}
};

// specialization for non-atom return type
template <typename R> struct
curry<std::function<R(atom)>> 
{
    using
    type = std::function<R(atom)>;

    const type
    result;

    curry(std::function<R(atom)> fun) : 
    result(
        [=](atom a){
            return curry<function>(
                [=](){
                    return atom(fun(a));
                }
            ).result;
        }
    ) 
    {}
};

// specialization for functions with a single argument
template <typename R, typename T> struct
curry<std::function<R(T)>> 
{
    using
    type = std::function<R(T)>;

    const type
    result;

    // all other argument types
    curry(std::function<R(T)> fun) : 
    result (
        [=](atom a) {
            return curry<std::function<R(atom)>>(
                [=](Ts&&...ts){
                    return fun(atom_cast<T>(a));
                }
            ).result;
        }
    ) 
    {}
};

// recursive atom specialization for functions with more arguments
template <typename R, typename...Ts> struct
curry<std::function<R(atom,Ts...)>> 
{
    using
    remaining_type = typename curry<std::function<R(Ts...)> >::type;

    using
    type = std::function<remaining_type(atom)>;

    const type
    result;

    // atom accepting variant
    curry(std::function<R(atom,Ts...)> fun) : 
    result(
        [=](atom a){
            return curry<std::function<R(Ts...)>>(
                [=](Ts&&...ts){ return fun(a, ts...); }
            ).result;
        }
    ) 
    {}
};

// recursive specialization for functions with more arguments
template <typename R, typename T,typename...Ts> struct
curry<std::function<R(T,Ts...)>> 
{
    using
    remaining_type = typename curry<std::function<R(Ts...)> >::type;

    using
    type = std::function<remaining_type(atom)>;

    const type
    result;

    curry(std::function<R(T,Ts...)> fun) : 
    result(
        [=](a){
            return curry<std::function<R(Ts...)>>(
                [=](Ts&&...ts){ return fun(atom_cast<T>(a), ts...); }
            ).result;
        }
    ) 
    {}
};
}

// curry takes any std::function or function pointer and returns a 
// std::function<atom(atom)>
template <typename R, typename... Ts> 
auto
curry(const std::function<R(Ts...)>& f)
-> typename detail::curry<std::function<R(Ts...)>>::type
{
    return detail::curry<std::function<R(Ts...)>>(f).result;
}

template <typename R, typename... Ts> 
auto
curry(R(* const f)(Ts...))
-> typename detail::curry<std::function<R(Ts...)>>::type
{
    return detail::curry<std::function<R(Ts...)>>(f).result;
}


namespace detail {
// final variations
atom eval_curried_function(const function& f, atom a){ return f(a); }
atom eval_curried_function(function&& f, atom a){ return f(a); }

// recursive variation
template <typename F>
atom eval_curried_function(F&& f, atom a)
{ 
    if(is_nil(a)){ return eval_curried_function(f(a),a); }
    else{ return eval_curried_function(f(car(a)),cdr(a)); }
}
}

// atomize_function converts any callable to an fl::function
template <typename R, typename... As>
function to_fl_function(std::function<R()> std_f)
{
    function final_f = [=](atom a) -> atom
    {
        return atom(std_f());
    };
    return final_f;
}

template <typename R, typename... As>
function to_fl_function(std::function<R(As...)> std_f)
{
    function final_f = [=](atom a) -> atom
    {
        auto curried_f = curry(std_f);
        return detail::eval_curried_function(curried_f,a); 
    };
    return final_f;
}

template <typename F>
function to_fl_function(F&& f)
{
    function final_f = [=](atom a) -> atom
    {
        auto std_f = to_std_function(std::forward<F>(f));
        auto curried_f = curry(std_f);
        return detail::eval_curried_function(curried_f,a); 
    };
    return final_f;
}



//-----------------------------------------------------------------------------
// atom  

class atom;
class print_map;

namespace detail {
template <typename T> class register_type; 

typedef bool(*compare_atom_function)(atom,atom)>;

template <>
bool compare_atom_function__(const atom& a, const atom& b){ return false; }

template <typename T>
bool compare_atom_function__(const atom& a, const atom& b)
{
    // a is guaranteed to be type T because this is used by the T templated 
    // atom constructor
    if(b.is<T>()){ return a.value<T>() == b.value<T>(): }
    else{ return false; }
}
}

#define REGISTER_TYPE__(T) detail::register_type<T>(#T)


class atom 
{
public:
    atom(){}
    atom(const atom& rhs) : a(rhs.ctx) {}
    atom(atom&& rhs) : a(std::move(rhs.ctx)) {}

    template <typename T>
    atom(T&& t){ set(std::forward<T>(t)); }

    // compare atom's real typed T values with the == operator 
    inline bool equalv(atom b) const { return caf(*this,b); }

    // allows direct value comparison such as:
    // a.equalv(3);
    // a.equalv("hello world");
    template <typename T>
    inline bool equalv(T&& t) const { return caf(*this,atom(std::forward<T>(t))); }

    bool operator==(const atom& rhs){ return equalv(rhs); }
    bool operator==(atom&& rhs){ return equalv(rhs); }
    template <typename T> bool operator==(T&& rhs){ return equalv(atom(std::forward<T>(rhs))); }

    inline bool equalp(atom b) const { return ctx.get() == b.ctx.get() }

    // atom_cast(), set() and extract() allow modification of the underlying 
    // std::any context. Modifying this will modify *ALL* atoms that have 
    // copies of the shared_ptr<context> 

    // any cast the atom's value. This is the most flexible and most dangerous 
    // way to get the value from an atom. Since this is a direct forward of 
    // std::any_cast it allows returning mutable references of the stored 
    // value.
    template <typename T>
    T atom_cast(){ return std::any_cast<T>(ctx->a); }

    // get atom's value as a const reference to the specified type
    template <typename T> 
    const unqualified<T>& 
    value() const { return std::any_cast<const unqualified<T>&>(ctx->a); }

    // set() is capable of changing the underlying stored type, not just the 
    // stored value.
    
    // c-string variant to ensure we can compare with std::string
    void set(const char* t) 
    { 
        static REGISTER_TYPE__(std::string);
        std::string s(t);
        ctx = make_atom_context(std::move(s), std::false_type());
    }

    template <typename T> 
    void set(T&& t) 
    { 
        static REGISTER_TYPE__(T);
        ctx = make_atom_context(std::forward<T>(t), std::is_function<unqualified<T>>());
    }

    atom& operator=(const atom& rhs)
    {
        ctx = rhs.ctx;
        return *this; 
    }

    atom& operator=(atom&& rhs)
    {
        ctx = std::move(rhs.ctx);
        return *this; 
    }

    template <typename T>
    atom& operator=(T&& rhs)
    {
        set(std::forward<T>(rhs));
        return *this; 
    }

    // Returns an rvalue reference allowing code to use move semantics to 
    // extract data from the internal context. This means that the data stored 
    // in the context will be in a valid but unknown state.
    template <typename T>
    unqualified<T>&& extract(){ return std::any_cast<unqualified<T>&&>(ctx->a); }

    // Make a deep copy of the current atom
    inline atom copy() const
    {
        atom b;
        *(b.ctx) = *(a.ctx);
        return b;
    }

    inline bool is_nil() const { return ctx ? false : true; }

    template <typename T>
    bool is() const
    {
        if(is_nil()){ return false; }
        else
        {
            try 
            {
                const auto& ref = value<T>(ctx->a);
                return true;
            }
            catch(...){ return false; }
        } 
    };

    // returns true if atom has a value, else false
    inline bool operator bool() const { return !is_nil(); }

private:
    struct atom_context 
    {
        std::any value;

        // the function pointer stored here knows what the stored type of value 
        // is, allowing sane/correct comparison
        detail::compare_atom_function caf;

        context(const context& rhs) : a(rhs.a), caf(rhs.caf) { }
        context(context&& rhs) : a(std::move(rhs.a)), caf(std::move(rhs.caf)) { }

        template <typename T>
        context(T&& t, detail::compare_any_function in_caf) : a(t), caf(in_caf) { }

        context& operator=(const context& rhs)
        {
            value = rhs.value;
            caf = rhs.caf;
            return *this;
        }

        context& operator=(context&& rhs)
        {
            value = std::move(rhs.value);
            caf = std::move(rhs.caf);
            return *this;
        }
    };

    mutable std::shared_ptr<atom_context> ctx;

    atom_context make_atom_context(T&& t, std::true_type) const
    {
        auto f = to_fl_function(std::forward<T>(t));
        return std::make_shared<context>(std::move(f),compare_atom_function__<>);
    }

    atom_context make_atom_context(T&& t, std::false_type) const
    {
        return std::make_shared<context>(std::forward<T>(t),compare_any_function__<T>);
    }

    friend class print_map;
};

inline atom nil(){ return atom(); }
inline bool is_nil(atom a){ return a.is_nil(); }
template <typename T> bool is(atom a){ return a.is<T>(); }
template <typename T> T atom_cast(){ return a.atom_cast<T>(); }
template <typename T> const T& value(atom a){ return a.value<T>(); }

inline bool equalv(atom lhs, atom rhs){ return lhs.equalv(rhs); }
template <typename T> bool equalv(atom lhs, T&& rhs){ return equalv(lhs, atom(std::forward<T>(rhs))); }
template <typename T> bool equalv(T&& lhs, atom rhs){ return equalv(atom(std::forward<T>(lhs),rhs)); }
template <typename T, typename T2> bool equalv(T&& lhs, T2&& rhs)
{ 
    return equalv(atom(std::forward<T>(lhs)),atom(std::forward<T2>(rhs))); 
}

inline bool equalp(atom lhs, atom rhs){ return lhs.equalp(rhs); }
inline atom copy(atom a){ return a.copy(); }
template <typename T> inline atom copy(T&& t){ return atom(std::forward<T>(t)); }
template <typename T> void set(atom a, T&& t){ a.set(std::forward<T>(t)); }
template <typename T> T&& extract(atom a){ return a.extract<T>(); }


//-----------------------------------------------------------------------------
// cons_cell 

namespace detail {
class cons_cell 
{
private:
    const atom car;
    const atom cdr;

public:
    cons_cell(){}
    cons_cell(const atom& a) : car(a) {}
    cons_cell(const atom& a, const atom& b) : car(a), cdr(b) {}
    cons_cell(atom&& a) : car(std::move(a)) {}
    cons_cell(atom&& a, atom&& b) : car(std::move(a)), cdr(std::move(b)) {}
    cons_cell(const atom& a, atom&& b) : car(a), cdr(std::move(b)) {}
    cons_cell(atom&& a, const atom& b) : car(std::move(a)), cdr(b) {}

    // Return a copy of internal car/cdr. 
    atom car() const { return atom(car); }
    atom cdr() const { return atom(cdr); }

    // returns true if cons_cell is nil, else false
    bool operator bool() const { return a.car || a.cdr; }
    bool operator==(const cons_cell& rhs) const { return equalv(car,rhs.car) && equalv(cdr,rhs.cdr); }
    bool operator==(cons_cell&& rhs) const { return equalv(car,rhs.car) && equalv(cdr,rhs.cdr); };
}

template <typename A, typename B>
inline atom cons(A&& a, B&& b)
{ 
    return atom(detail::cons_cell(atom(std::forward<A>(a)), 
                                  atom(std::forward<B>(b)))); 
}

inline bool is_cons(atom a){ return is<detail::cons_cell>(a); }
inline atom car(atom a){ return value<detail::cons_cell>(a).car(); }
inline atom cdr(atom a){ return value<detail::cons_cell>(a).cdr(); }



//-----------------------------------------------------------------------------
// list  
namespace detail {
template <typename A>
atom list_(A&& a){ return cons(std::forward<A>(a),nil()); }

template <typename A, typename... As>
atom list_(A&& a, As&&... as)
{
    return cons(std::forward<A>(a), list_(std::forward<As>(as)...));
}
} // end detail

template <typename T, typename... Ts>
atom list(T&& t, Ts&&... Ts)
{
    return detail::list_(std::forward<T>(t),std::forward<Ts>(ts)...);
}

struct list_info
{
    bool is_list;
    size_t length;
    list_info(){}
    list_info(bool in_is_list, size_t in_length) : 
        is_list(in_is_list), 
        length(in_length)
    {}
};

inline list_info inspect_list(atom a)
{
    size_t sz=0;
   
    while(true)
    {
        if(is_cons(a))
        {
            detail::cons_cell& p = value<detail::cons_cell>(a);
            if(p)
            { 
                ++sz;
                a = a.cdr; 
            }
            else{ return list_info(true,sz); } // is list
        }
        else{ return list_info(false,sz); }
    }
}

inline bool is_list(atom a){ return inspect_list(a).is_list; }
inline size_t length(atom a){ return inspect_list(a).length; }

// cons access 
inline atom head_cons(atom lst){ return lst }
inline atom tail_cons(atom lst)
{
    atom ret = lst;
    while(lst)
    { 
        ret = lst;
        lst = cdr(lst):
    }
    return ret;
}

// return the cons cell in a list at index position n
inline atom nth_cons(atom lst, size_t n)
{ 
    while(n)
    {
        --n;
        lst = cdr(lst);
    }
    return lst;
}

// element access
inline atom head(atom lst){ return car(lst); }
inline atom tail(atom lst){ return car(tail_cons(lst)); }

// return the element in a list at index n
inline atom nth(atom lst, size_t n){ return car(nth_cons(lst,n)); }

// given a list, return a list in reverse order
inline atom reverse(atom lst)
{
    if(is_nil(lst)){ return lst; }
    else 
    {
        atom ret = cons(car(lst),nil());
        lst = cdr(lst);
        while(!is_nil(lst))
        {
            ret = cons(car(lst),ret); // build ret in reverse
            lst = cdr(lst);
        }
        return ret;
    }
}

/*
// return a value copy of a list
inline atom copy_list(atom lst)
{
    atom ret; // nil
    atom cur = ret;
    if(is_nil(lst)){ return meta_list(ret,cur,0); } 
    else 
    {
        atom ret = cons(copy(car(lst)), copy(cdr(lst)));
        atom cur = cdr(ret);
        lst = cdr(lst);
        while(!is_nil(lst))
        {
            cur = cons(copy(car(lst)), copy(cdr(lst)));
            cur = cdr(cur);
            lst = cdr(lst);
        }
        copy(cdr(cur),nil());
        return ret;
    }
}
*/

// copies any structure composed of cons_cells and any other atom
inline atom copy_tree(atom lst)
{
    /* ... */
}


namespace detail {
atom append_(atom lst1, atom lst2)
{
    atom meta_lst = detail::copy_list(lst1);
    *(meta_lst.tail.a) = lst2; // do not make a copy of tail
    return meta_lst.head;

}

template <typename... As>
atom append_(atom lst1, atom lst2, As&&... as)
{
    atom meta_lst = detail::copy_list(lst1);
    *(meta_lst.tail.a) = append_(lst2, as...);
    return meta_lst.head;
}
}

template <typename... As>
atom append(atom a, atom b, As&&... as)
{
    return detail::append_(a,b,as);
}


//TODO: need to implement the following list functions 
/*
 list_with_tail: list* in racket
 build_list
 */



//-----------------------------------------------------------------------------
// quote
namespace detail {
class quote{};
}

inline atom quote(atom a){ return cons(atom(detail::quote()),a); }
inline bool is_quote(atom a){ return is<detail::quote>(a); }

inline atom unquote(atom a)
{
    if(is_quote(car(a))
    {
        do{ a = cdr(a); } while(is_quote(car(a)));
        return a;
    }
    else{ return a; }
}



//-----------------------------------------------------------------------------
// eval 

// eval allows data to be treated as code, assuming that the given atom is a
// fl::function to be executed. 
inline atom eval(atom a)
{ 
    if(a && is<const function&>(a))
    {
        return value<const function&>(car(a))(cdr(a)); 
    }
    else{ return a; }
}


template <typename F, typename... Ts>
inline atom eval(F&& f, Ts&&... ts)
{ 
    return eval(list(std::forward<F>(f),std::forward<Ts>(ts)...));
}



//-----------------------------------------------------------------------------
// atom printing 


namespace detail {
class print_map
{
public:
    print_map& instance();
    
    inline std::pair<std::string,std::string> get_value_info(atom a)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        auto& vi = type_map_[get_std_type_name(a)];
        return std::pair<std::string,std::string>(vi.name,vi.pr(a));
    }

    inline std::string get_type(atom a)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        return type_map_[get_std_type_name(a)].name;
    }

    inline std::string get_value(atom a)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        return type_map_[get_std_type_name(a)].pr(a_ref);
    }

private:
    typedef std::function<std::string(std::any&)> value_printer;

    std::mutex mtx_;
    std::unordered_map<std::string,value_info> type_map_;

    struct value_info
    {
        std::string name;
        value_printer vp;
    };

    std::string get_std_type_name(atom a){ return a.ctx->a.type().name(); }
   
    //integral to_string conversion
    template <typename T,
              typename = std::enable_if_t<std::is_integral<T>::value>>
    value_printer make_value_printer(T& t)
    {
        value_printer vp = [](std::any& a) -> std::string 
        {
            const auto& v = std::static_cast<const T&>(a);
            return std::to_string(v);
        };
        return vp;
    }
   
    //float to_string conversion
    template <typename T,
              typename = std::enable_if_t<std::is_floating_point<T>::value>>
    value_printer make_value_printer(T& t)
    {
        value_printer vp = [](std::any& a) -> std::string 
        {
            const auto& v = std::static_cast<const T&>(a);
            return std::to_string(v);
        };
        return vp;
    }

    //direct string conversion
    value_printer make_value_printer(const std::string& t)
    {
        value_printer vp = [](std::any& a) -> std::string 
        {
            const auto& v = std::static_cast<const std::string&>(a);
            return std::string("\"")+std::string(v)+std::string("\"");
        };
        return vp;
    }

    value_printer make_value_printer(const char* t)
    {
        value_printer vp = [](std::any& a) -> std::string 
        {
            const auto& v = std::static_cast<const char*>(a);
            return std::string("\"")+std::string(v)+std::string("\"");
        };
        return vp;
    }

    // address only
    template <typename T>
    value_printer make_value_printer(T& t)
    {
        value_printer vp = [](std::any& a) -> std::string 
        {
            size_t p = (void*)&(std::any_cast<const T&>(a));
            std::stringstream ss;
            ss << std::hex << p;
            return std::string(ss.str());
        };
        return vp;
    }

    template <typename T> 
    void register_type(const char* name)
    { 
        unqualified<T> t;
        std::any a(t);
        std::string std_type_name = a.type().name();

        std::unique_lock<std::mutex> lk(mtx_); 

        // don't modify an already registered type
        auto it = type_map_.find(std_type_name);
        if(it == type_map_.end())
        {
            value_info vi;
            vi.name = std::string(name);
            vi.pr = make_value_printer(t);
            type_map_[std_type_name] = std::move(vi);
        }
    } 

    friend template <typename T> class register_type;
};

template <typename T>
class register_type
{
public:
    register_type(const char* name)
    {
        detail::print_map::instance()->register_type<T>(name);
    }
};
}
inline std::string atom_name(atom a){ return detail::print_map::instance()->get_type(a); }
inline std::string atom_value(atom a){ return detail::print_map::instance()->get_value(a); }

// returns a name,value pair
inline std::pair<std::string,std::string> atom_type_info(atom a)
{ 
    return detail::print_map::instance()->get_value_info(a); 
}


inline std::string to_string(atom a);

namespace detail {
inline std::string to_string1(atom a)
{
    if(is_cons(a)){ return to_string(a); }
    else if(is_quote(a)){ return std::string("'"); }
    else if(is_nil(a)){ return std::string("nil"); }
    else
    { 
        auto ti = atom_type_info(a);
        return ti.first+":"+ti.second; 
    }
}
}

inline std::string to_string(atom a)
{ 
    std::string s;
    if(is_cons(a))
    {
        if(is_quote(car(a))){ s+=std::string("'("); }
        else 
        {
            s+=std::string("("); 
            s+=detail::to_string1(car(a));
        }

        a = cdr(a);

        while(!is_nil(a))
        {
            s+=std::string(" "); 
            s+=detail::to_string1(a);
            a = cdr(a);
        }
        s+=std::string(")");
    }
    else{ s+=detail::to_string1(a); }
    return s;
}



//-----------------------------------------------------------------------------
// iteration
namespace detail {
void iterate_(){ return; }

template <typename A, typename As...>
void iterate_(A& a, As&... as)
{ 
    a = cdr(a);
    iterate_(as...); 
}
}

// map
template <typename A, typename As...>
atom map(A f, A a, A... as)
{
    atom ret; // default atom is nil 
    while(!is_nil(a))
    {
        ret = cons(eval(f,list(car(a),car(as)...)), ret);
        iterate_(a,as...);
    }
    return std::move(a);
}

// map length
template <typename F, typename A, typename As...>
atom mapl(F&& f, size_t len, A a, A... as)
{
    atom ret; // default atom is nil
    while(len)
    {
        --len;
        ret = cons(eval(f,list(car(a),car(as)...)), ret);
        iterate_(a,as...);
    }
    return std::move(a);
}

// fold left 
template <typename F, typename As...>
atom foldl(F&& f, atom init, atom a, As... as)
{
    while(!is_nil(a))
    {
        init = eval(f,init,list(car(a),car(as)...));
        iterate_(a,as...);
    }
    return init;
}

// fold left-to-right length 
template <typename F, typename As...>
atom foldll(F&& f, size_t len, atom init, atom a, As... as)
{
    while(len)
    {
        --len
        init = eval(f,init,list(car(a),car(as)...));
        iterate_(a,as...);
    }
    return init;
}

// fold right-to-left
template <typename F, typename As...>
atom foldr(F&& f, atom init, atom a, As... as)
{
    return foldl(std::forward<F>(f),reverse(a),reverse(as)...);
}

// fold right-to-left length
template <typename F, typename As...>
atom foldrl(F&& f, size_t len, atom init, atom a, As... as)
{
    return foldll(std::forward<F>(f),len,reverse(a),reverse(as)...);
}

//TODO: implement the following (racket) iterating algorithms:
/*
 andmap
 andmapl
 ormap
 ormapl
 for_each
 filter
 remove 
 remv
 remp
 remove_set
 remsetv
 remsetp
 sort
 member
 memv
 memp
 memf
 findf
 assoc
 assv
 assp
 assf
 */



//-----------------------------------------------------------------------------
// std:: container conversions

// convert an allocator aware container to an fl::list containing all elements 
// of said container. The elements are returned in the reverse order traversed 
// over the container, the list must be reverse()d if order matters (although 
// rebuilding a list via cons() during foldl will have the same effect with the 
// extra functionality of operating on the data).
template <typename C>
atom atomize_container(C&& c, size_t idx=0, size_t len=0)
{
    const bool is_rvalue = std::is_rvalue_reference<C&&>::value;
    typedef typename std::remove_cv<C> UC; // unqualified C
    typedef typename UC::iterator IT;

    IT cur=c.begin();
    IT end;
    atom lst=nil();

    if(len){ end = std::next(cur); }
    else{ end=c.end() };

    if(is_rvalue)
    {
        for(; cur!=end; ++cur)
        { 
            lst = cons(car(lst), cons(p(std::move(*cur),nil())));
        }
    } 
    else 
    {
        for(; cur!=end; ++cur)
        { 
            lst = cons(car(lst), cons(p(*cur,nil())));
        }
    }

    return lst;
}

// convert an fl::atom list to an allocator aware, size constructable container. 
template <typename C>
C reconstitute_container(atom a)
{
    typedef typename C::value_type T;
    const bool is_rvalue = std::is_rvalue_reference<A&&>::value;

    auto linfo = inspect_list(a);
    if(linfo.is_list)
    {
        C c(linfo.length);

        auto cur = c.begin();
        auto end = c.end();

        if(is_rvalue)
        {
            while(; cur!=end; ++cur)
            {
                auto& c = value<detail::cons_cell>(a);
                *cur = value<T>(c.car);
                a = c.cdr;
            }
        }
        else
        {
            while(; cur!=end; ++cur)
            {
                auto& c = value<detail::cons_cell>(a);
                *cur = std::move(value<T>(c.car));
                a = c.cdr;
            }
        }

        return std::move(c);
    }
    else{ return C(); }
}



//-----------------------------------------------------------------------------
//  std:: compatibility

// fl::iterator for fl::list organizations of atoms, for use in std:: 
// algorithms
class iterator : public std::forward_iterator_tag
{
public:
    typedef value_type atom;

    iterator() : a(atom(detail::cons_cell())) {} // nil
    iterator(atom ia) : a(ia) {}
    iterator(const iterator& rhs) : a(rhs.a) {}
    iterator(iterator&& rhs) : a(std::move(rhs.a)) {}

    iterator& operator=(const iterator& rhs) const
    {
        a = rhs.a;
        return *this;
    }

    iterator& operator=(iterator&& rhs) const
    {
        a = std::move(rhs.a);
        return *this;
    }

    iterator& operator++() const
    {
        if(is_cons(a) && !is_nil(a)){ a = a.cdr; }
        return *this;
    }

    iterator operator++(int unused) const 
    {
        iterator ret(*this);
        if(is_cons(a) && !is_nil(a)){ a = a.cdr; }
        return ret;
    }

    bool operator==(const iterator& rhs) const { return compare(a, rhs.a); }
    bool operator==(iterator&& rhs) const { return compare(a, rhs.a); }
    bool operator!=(const iterator& rhs) const { return !compare(a, rhs.a); }
    bool operator!=(iterator&& rhs) const { return !compare(a, rhs.a); }

    atom& operator*() const { return a; }
    atom* operator->() const { return &a; }

private: 
    mutable atom a;
};

class const_iterator : public std::forward_iterator_tag
{
public:
    typedef value_type atom;

    const_iterator() : a(atom(detail::cons_cell())) {} // nil
    const_iterator(atom ia) : a(ia) {}
    const_iterator(const const_iterator& rhs) : a(rhs.a) {}
    const_iterator(const_iterator&& rhs) : a(std::move(rhs.a)) {}

    const_iterator& operator=(const const_iterator& rhs) const
    {
        a = rhs.a;
        return *this;
    }

    const_iterator& operator=(const_iterator&& rhs) const
    {
        a = std::move(rhs.a);
        return *this;
    }

    const_iterator& operator++() const
    {
        if(is_cons(a) && !is_nil(a)){ a = a.cdr; }
        return *this;
    }

    const_iterator operator++(int unused) const 
    {
        const_iterator ret(*this);
        if(is_cons(a) && !is_nil(cdr(a))){ a = cdr(a); }
        return ret;
    }

    bool operator==(const const_iterator& rhs) const { return compare(a, rhs.a); }
    bool operator==(const_iterator&& rhs) const { return compare(a, rhs.a); }
    bool operator!=(const const_iterator& rhs) const { return !compare(a, rhs.a); }
    bool operator!=(const_iterator&& rhs) const { return !compare(a, rhs.a); }

    const atom& operator*() const { return a; }
    const atom* operator->() const { return &a; }

private: 
    mutable atom a;
};
} // end fl



//-----------------------------------------------------------------------------
// overload for std::begin 
namespace std {
iterator begin(fl::atom a){ return iterator(fl::value<fl::detail::cons_cell>(a).car); }
iterator end(fl::atom a){ return iterator(); }

iterator cbegin(fl::atom a){ return const_iterator(fl::value<fl::detail::cons_cell>(a).car); }
iterator cend(fl::atom a){ return const_iterator(); }
}



//-----------------------------------------------------------------------------
// channel 

// channel is an interface (via std::shared_ptr) to an internal mechanism for 
// sending atoms and retrieving atoms from a threadsafe queue. 

namespace fl {

class channel
{
public:
    inline channel(){}
    inline channel(const channel& rhs) : ctx(rhs.ctx) {}
    inline channel(channel&& rhs) : ctx(std::move(rhs.ctx)) {}

    inline channel& operator=(const channel& rhs)
    {
        ctx = rhs.ctx;
        return *this;
    }

    inline channel& operator=(channel&& rhs)
    {
        ctx = std::move(rhs.ctx);
        return *this;
    }

    bool operator bool(){ return ctx ? true : false; }

    inline void make(){ ctx = std::make_shared<channel_context>(); }
    inline void close(){ return ctx->close(); }
    inline bool closed(){ return ctx->closed(); }
    inline size_t size(){ return ctx->size(); }
    inline bool send(atom a){ return ctx->send(a); }
    inline bool recv(atom& a){ return ctx->recv(a); }

    template <typename T>
    bool send(T&& t){ return ctx->send(atom(std::forward<T>(t))); }

    template <typename T>
    bool recv(T& t)
    { 
        atom a(std::move(t));
        bool success = ctx->recv(a); 
        t = extract<T>(a);
        return success;
    }

private:
    struct channel_context 
    {
    public:
        inline channel_context() : closed(false) {}

        inline void close()
        {
            std::unique_lock<std::mutex> lk(mtx);
            closed=true;
            non_empty_cv.notify_all();
        }

        inline bool closed()
        {
            std::unique_lock<std::mutex> lk(mtx);
            return closed;
        }

        inline size_t size()
        {
            std::unique_lock<std::mutex> lk(mtx);
            return lst.size();
        }

        inline bool send(atom a)
        {
            atom s = copy_tree(a);
            std::unique_lock<std::mutex> lk(mtx);
            if(!closed)
            {
                lst.push_back(s);
                non_empty_cv.notify_one();
                return true;
            }
            else{ return false; }
        }

        inline bool recv(atom& a)
        {
            std::unique_lock<std::mutex> lk(mtx);
            while(!closed && lst.size()){ non_empty_cv.wait(lk); }
            if(!closed)
            {
                a = lst.front();
                lst.pop_front();
                return true;
            }
            else{ return false; }
        }

    private:
        std::mutex mtx;
        std::condition_variable non_empty_cv;
        std::list<atom> lst;
        bool closed;
    };

    std::shared_ptr<channel_context> ctx;
};

channel make_channel(size_t capacity=1)
{ 
    channel c;
    c.make(capacity);
    return c;
}



//-----------------------------------------------------------------------------
// worker 
//
// worker is an interface to an std::thread stored in an std::shared_ptr.
//
// The std::thread will continually call its function f on atoms received from 
// its schedule() call. This will continue until the worker's context 
// std::shared_ptr goes out of scope or halt() is called, upon which calls to f 
// will cease, the worker function will return and the thread joined.
//
// Example:
/*
#include <fl/fl.hpp>

struct message
{
    size_t command;
    atom payload;
    
    message(size_t in_command) : command(in_command) { }
    message(size_t in_command, atom in_payload) : command(in_command), payload(in_payload) { }
};

int main()
{
    channel done_ch = make_channel();

    // w calls our custom function with whatever we send it with its schedule() 
    // function
    worker w([=](message& m)
    {
        switch(m.command)
        {
        case 0:
            eval(m.payload);
            break;
        case 1:
            eval([](std::string s){ std::cout << s << std::endl; }, m.payload);
            break;
        case 2:
            std::cout << "world" << std::endl; 
            break;
        case 3:
        default:
            current_worker().halt(); // shutdown w
            done_ch.send(0); // send done message back to main()
            break;
        }
    });

    w.schedule(message(0,[]{ std::cout << 3 << std::endl; }));
    w.schedule(message(1,"hello"));
    w.schedule(message(2));
    w.schedule(message(3));

    int done;
    done_ch.recv(done);

    return 0;
}
 */

class workerpool; // forward declaration

class worker 
{
public:
    inline worker(){}
    inline worker(const worker& rhs) : ctx(rhs.ctx) { }
    inline worker(worker&& rhs) : ctx(std::move(rhs.ctx)) { }
    inline worker(function f){ start(this,f); }
    template <typename f> worker(F&& f){ start(this,std::forward<F>(f)); }

    inline worker& operator=(const worker& rhs)
    {
        ctx = rhs.ctx;
        return *this;
    }

    inline worker& operator=(worker&& rhs)
    {
        ctx = std::move(rhs.ctx);
        return *this;
    }

    bool operator bool(){ return ctx ? true : false; }

    inline void start(function f){ ctx = std::make_shared<worker_context>(this,f); }

    template <typename F>
    inline void start(F&& f)
    {
        start(atomize_function(std::forward<F>(f)));
    }

    inline bool halt(){ ctx->halt(); }

    inline void schedule(atom a){ return ctx->schedule(a); }

    template <typename F, typename... Ts>
    void schedule(F&& f, Ts&&... ts)
    {
        return schedule(list(std::forward<F>(f),std::forward<Ts>(ts)...));
    }

private:
    struct worker_context 
    {
    public:
        inline worker_context(worker* parent_w, function f) : parent_tp(nullptr), ch(make_channel())
        {
            run = std::make_shared<bool>(false);
            done = std::make_shared<bool>(false);
            thd = std::thread([&](function f)
            {
                worker_context* parent_worker = get_current_worker();
                set_current_worker(parent_w);
                try 
                {
                    bool recv_success=false;
                    std::unique_lock<std::mutex> lk(mtx);
                    *run=true;
                    ready_cv.notify_one();
                    atom a;
                    while(*run)
                    {
                        lk.unlock();
                        recv_success = ch.recv(a);
                        if(recv_success)
                        { 
                            f(a); 
                            lk.lock();
                        }
                        else 
                        {
                            lk.lock();
                            *run=false; // shutdown if channel is closed
                        }
                    }
                }
                catch(...)
                {
                    *done=true;
                    set_current_worker(parent_worker);
                    done_cv.notify_one();
                    std::rethrow_exception(std::current_exception());
                }
                *done=true;
                set_current_worker(parent_worker);
                done_cv.notify_one();
            }, f);

            std::unique_lock<std::mutex> lk(mtx);
            while(!*run){ ready_cv.wait(lk); }
        }

        worker* get_current_worker();
        void set_current_worker(worker* w);

        inline void schedule(atom a){ return ch.send(a); }
        inline void halt()
        { 
            std::unique_lock<std::mutex> lk(mtx);
            if(thd.joinable())
            {
                *run = false;
                ch.close();
                while(!*done){ done_cv.wait(lk); }
                lk.unlock();
                thd.join();
            }
        }

        inline bool in_workerpool(){ return parent_tp ? true : false; }
        inline workerpool* get_workerpool(){ return *parent_tp; }

        inline ~worker_context(){ halt(); }

    private:
        std::mutex mtx;
        std::condition_variable ready_cv;
        std::condition_variable done_cv;
        std::thread thd;
        std::shared_ptr<bool> run;
        std::shared_ptr<bool> done;
        channel ch;
        workerpool* parent_tp;
        friend class workerpool;
    };

    std::shared_ptr<worker_context> ctx;
    friend class workerpool;
};

bool in_worker();

// Keeping a copy of a worker (for instance, in an atom) can cause the worker to 
// never go out of scope, causing a memory leak. Try to always use this function 
// inline instead of caching it. 
//
// An exception to this is when the lifetime of the copy is temporary, such as 
// when creating a worker to execute an io call, whereupon a cached 
// current_worker() value is used to re-schedule the continuation.
worker current_worker();



//-----------------------------------------------------------------------------
// workerpool
//
// workerpool is an interface class to a shared_ptr context of managed worker 
// objects. A workerpool will create N count of workers when its start() 
// function is called, and will schedule atoms for evaluation to said workers 
// when its schedule() function is called.
//
// All workers created with workerpool are passed the function eval(). Thus, 
// workers in a workerpool are very generic, and are best used when efficient,
// asynchronous computation is required.
//
// All workers will be halt()ed when the last workerpool to the shared context 
// goes out of scope.

class workerpool 
{
public:
    inline workerpool(){}
    inline workerpool(const workerpool& rhs) : ctx(rhs.ctx) { }
    inline workerpool(workerpool&& rhs) : ctx(std::move(rhs.ctx)) { }

    inline workerpool& operator=(const workerpool& rhs)
    {
        ctx = rhs.ctx;
        return *this;
    }

    inline workerpool& operator=(workerpool&& rhs)
    {
        ctx = std::move(rhs.ctx);
        return *this;
    }

    bool operator bool(){ return ctx ? true : false; }

    inline void start(size_t worker_count=std::thread::hardware_concurrency())
    {
        ctx = std::make_shared<workerpool_context>(this, worker_count);
    }

    inline void halt(){ ctx->halt(); }

    inline size_t worker_count(){ return ctx->worker_count(); }

    inline void schedule(atom a){ return ctx->schedule(a); }

    template <typename F, typename... Ts>
    void schedule(F&& f, Ts&&... ts)
    {
        return schedule(list(std::forward<F>(f),std::forward<Ts>(ts)...));
    }

private:
    struct workerpool_context 
    {
    public:
        inline workerpool_context(workerpool* parent_tp, 
                                  size_t worker_count) :
            ch(make_channel())
        { 
            if(!worker_count){ worker_count=1; }

            workers.reserve(worker_count);
            for(size_t i=0; i<worker_count; ++i)
            { 
                workers.emplace_back(eval,make_channel());
                workers.back().ctx->parent_tp = this;
            }

            cur = workers.begin();
            end = workers.end();
        }

        inline void schedule(atom a)
        {
            std::unique_lock<std::mutex> lk(mtx);
            cur->schedule(a);
            // rotate which worker to schedule on next
            if(cur==end){ cur = workers.begin(); }
            else{ ++cur; }
            return nil();
        }

        inline size_t worker_count()
        { 
            std::unique_lock<std::mutex> lk(mtx);
            return workers.size(); 
        }

        inline void halt()
        {
            std::unique_lock<std::mutex> lk(mtx);
            for(auto cur = workers.begin(); cur!=workers.end(); ++cur)
            {
                cur->halt();
            }
        }

        ~workerpool_context(){ halt(); }

    private:
        std::mutex mtx;
        channel ch;
        std::vector<worker> workers;
        std::vector<worker>::iterator cur;
        std::vector<worker>::iterator end;
        workerpool* parent_tp;
        workerpool* previous_tp;
    };

    std::shared_ptr<workerpool_context> ctx;
};

bool in_workerpool();

// keeping a copy of the workerpool (for instance, in an atom) can cause the 
// workerpool to never go out of scope, causing a memory leak. Try to always 
// use this function inline instead of caching it.
workerpool current_workerpool(); 

// set the worker thread count (only effective if default_workerpool() has not 
// yet been called, as that function forces initialization).
void set_default_workerpool_worker_count(size_t cnt);

// keeping a copy of the workerpool (for instance, in an atom) can cause the 
// workerpool to never go out of scope, causing a memory leak. Try to always 
// use this function inline instead of caching it.
workerpool default_workerpool();



//-----------------------------------------------------------------------------
// schedule an atom for execution on a worker thread in either the current 
// workerpool, the current worker, or the default workerpool, in that order,
// as available.
atom schedule(atom a);

template <typename F, typename... Ts>
atom schedule(F&& f, Ts&&... ts)
{
    return schedule(list(std::forward<F>(f),std::forward<Ts>(ts)...));
}



//-----------------------------------------------------------------------------
// continuation 
//
// A continuation is a special channel which implements non-blocking behavior by 
// scheduling functions (continuations) that will continue the current operation 
// when a send/recv can succeed. This is a variation of a technique known in the 
// lisp world as "continuation passing" style.
//
// In practice, this behavior is essentially an implementation of "coroutines".
// Fully supported coroutines can implement this behavior by allocating an 
// entire stack (similar to a thread's call stack) where the coroutine can 
// execute. This behavior is emulated here not through an allocated call stack, 
// but by using atoms for their default shared_ptr behavior. 
//
// This non-blocking behavior is *extremely* efficient, it effectively allows 
// regular functions to behave like threads, yet actually executing on a small
// number of worker threads (typically identical in number to the count of 
// available CPUs), maximizing cpu usage while minimizing the cost of context 
// switching.
//
// When a continuation::send(send_val, send_cont) OR
// continuation::recv(recv_cont) succeeds it will schedule send_cont for 
// evaluation with send_val as an argument, while scheduling recv_cont with a 
// copy (default copy function is a deep copy with copy_tree()) of send_val. 
// An alternate fl::function can be provided to make() as the copy function if 
// necessary for efficiency.
//
// An alternate send(send_val) variant is available if no continuation is 
// required by the sending code.
//
// If there are no receivers when a send() occurs the send operation will be 
// cached until the next recv() call. The same is true if there are no senders 
// when a receive occurs until the next send() call.
//
// example:
/*
#include <fl/fl.hpp>

int main()
{
    fl::continuation cn = fl::make_continuation();
    fl::channel ch = fl::make_channel();

    // sender 
    fl::atom send_f = [=](int i)
    {
        if(i<2){ cn.send(++i,send_f); }
        else{ ch.send(0); }
    };
    
    // receiver 
    fl::atom recv_f = [=](int i)
    {
        if(i<2)
        {
            std::cout << i << std::endl;
            cn.recv(recv_f);
        }
        else{ ch.send(0; }
    };

    cn.send(-1, send_f);
    cn.recv(recv_f);

    // synchronize with worker functions
    i=0;
    ch.recv(i);
    ch.recv(i);
    return 0;
}
// program output should be:
// 0
// 1
// 2
 */
class continuation 
{
public:
    continuation(){}
    continuation(const continuation& rhs) : ctx(rhs.ctx) {}
    continuation(continuation&& rhs) : ctx(std::move(rhs.ctx)) {}

    inline continuation& operator=(const continuation& rhs)
    {
        ctx = rhs.ctx;
        return *this;
    }

    inline continuation& operator=(continuation&& rhs)
    {
        ctx = std::move(rhs.ctx);
        return *this;
    }

    inline bool operator bool(){ return ctx ? true : false; }
    inline void make(){ ctx = std::make_shared<continuation_context>(); }
    inline void make(atom copy_func){ ctx = std::make_shared<continuation_context>(copy_func); }

    template <typename F>
    void make(F&& copy_func){ make(atom(std::forward<F>(copy_func))); }

    inline void close(){ ctx->close(); }
    inline bool close(){ return ctx->closed(); }
    inline bool send(atom val, atom cont){ return ctx->send(val,cont); }
    inline bool send(atom val){ return ctx->send(val); }
    inline bool recv(atom& cont){ return ctx->recv(cont); }

    template <typename T, typename F> 
    bool send(T&& t, F&& f)
    { 
        return ctx->send(atom(std::forward<T>(t)), atom(std::forward<F>(f))); 
    }

    template <typename T, typename F> 
    bool send(T&& t)
    { 
        return ctx->send(atom(std::forward<T>(t))); 
    }

    template <typename T, typename F> 
    bool recv(F&& f)
    {  
        return ctx->recv(atom(std::forward<F>(f))); 
    }

private:
    struct continuation_context 
    {
        continuation_context() : closed(false), copy_func(copy_tree) {}
        continuation_context(atom in_copy_func) : closed(false), copy_func(in_copy_func) {}

        inline void close()
        {
            std::unique_lock<std::mutex> lk(mtx);
            closed=true;
        }

        inline bool closed()
        {
            std::unique_lock<std::mutex> lk(mtx);
            return closed;
        }

        inline bool send(atom send_val, atom send_cont)
        { 
            atom recv_val = eval(list(copy_func,send_val));

            std::unique_lock<std::mutex> lk(mtx);
            if(closed){ return false; }
            else
            {
                if(receivers.size())
                {
                    atom recv_cont = receivers.front();
                    receivers.pop_front();
                    atom send_val = car(s);
                    atom send_cont = cdr(s);
                    schedule(atom([=]{ eval(list(recv_cont, recv_val)); }));
                    schedule(atom([=]{ eval(list(send_cont, send_val)); }));
                }
                else{ senders.push_back(cons(recv_val,cons(send_val,send_cont))); }
                return true;
            }
        }

        inline bool send(atom send_val){ return send(send_val,atom([]{}); }

        inline bool recv(atom recv_cont)
        { 
            std::unique_lock<std::mutex> lk(mtx);
            if(closed){ return false; }
            else
            {
                if(senders.size())
                {
                    atom s = senders.front();
                    senders.pop_front();
                    atom recv_val = car(s);
                    atom ccell = cdr(s);
                    atom send_val = car(ccell);
                    atom send_cont = cdr(ccell);
                    schedule(atom([=]{ eval(list(recv_cont, recv_val)); }));
                    schedule(atom([=]{ eval(list(send_cont, send_val)); }));
                }
                else{ receivers.push_back(recv_cont); }
                return true;
            }
        }

        std::mutex mtx;
        std::condition_variable cv;
        bool closed;
        std::list<atom> senders;
        std::list<atom> receivers;
        atom copy_func;
    };

    std::shared_ptr<continuation_context> ctx;
};

continuation make_continuation()
{
    continuation cn;
    cn.make();
    return cn;
}

continuation make_continuation(atom copy_func)
{
    continuation cn;
    cn.make(copy_func);
    return cn;
}

template <typename F>
continuation make_continuation(F&& copy_func)
{
    continuation cn;
    cn.make(atom(std::forward<F>(copy_func)));
    return cn;
}



//-----------------------------------------------------------------------------
// io  
//
// io is similar to continuation in that io() itself is non-blocking. io() will 
// launch a new thread that will execute a function (io_call) after which, if a 
// function cont was provided, it will schedule() cont on the original worker 
// io() was called from.
bool io(atom io_call, atom cont)
{
    if(in_worker())
    {
        worker cw = current_worker();
        worker w(eval);
        w.schedule([=]
        {
            eval(io_call);
            cw.schedule(cont);
            current_worker().halt();
        });
        return true;
    }
    else{ return false; }
}

template <typename IO, typename CONT>
void io(IO&& io_f, CONT&& cont_f)
{
    io(list(std::forward<IO>(io_f),std::forward<CONT>(cont_f)));
}

bool io(atom io_call)
{
    if(in_worker())
    {
        worker w(eval);
        w.schedule([=]
        {
            eval(io_call);
            current_worker().halt();
        });
        return true;
    }
    else{ return false; }
}

template <typename IO, typename CONT>
void io(IO&& io_f)
{
    io(list(std::forward<IO>(io_f)));
}

} // end fl

#endif
