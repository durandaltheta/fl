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
                    return fun(value<T>(a));
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
                [=](Ts&&...ts){ return fun(value<T>(a), ts...); }
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
// apply atoms one at a time to curried function
atom eval_curried_function(const function& f, atom a){ return f(a); }
atom eval_curried_function(function&& f, atom a){ return f(a); }

// recursive variation
template <typename F>
atom eval_curried_function(F&& f, atom a){ return eval_curried_function(f(car(a)),cdr(a)); }
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
    atom(T&& t){ set(std::forward<T>(t); }

    // compare atom's real typed T values with the == operator 
    inline bool equalv(atom b) const { return caf(*this,b); }

    // allows direct value comparison such as:
    // a.equalv(3);
    // a.equalv("hello world");
    template <typename T>
    inline bool equalv(T&& t) const { return caf(*this,atom(std::forward<T>(t))); }

    inline bool equalp(atom b) const { return ctx.get() == b.ctx.get() }

    bool is_nil() const { return ctx ? false : true; }

    template <typename T>
    bool is() const
    {
        if(is_nil()){ return false; }
        else
        {
            try 
            {
                const auto& ref = std::any_cast<const unqualified<T>&>(ctx->a);
                return true;
            }
            catch(...){ return false; }
        } 
    };

    // get atom's value as a const reference to the specified type
    template <typename T> 
    const unqualified<T>& 
    value() const { return std::any_cast<const unqualified<T>&>(ctx->a); }

    inline atom copy() const
    {
        atom b;
        *(b.ctx) = *(a.ctx);
        return b;
    }

    // set() functions and extract() are only way to reset or modify the 
    // underlying context. Modifying this will modify *ALL* atoms that have 
    // copies of the current shared_ptr<context> 
    
    // c-string variant to ensure we can compare with std::string
    void set(const char* t) 
    { 
        static REGISTER_TYPE__(std::string);
        std::string s(t);
        ctx = make_context(std::move(s), std::false_type());
    }

    template <typename T> 
    void set(T&& t) 
    { 
        static REGISTER_TYPE__(T);
        ctx = make_context(std::forward<T>(t), std::is_function<unqualified<T>>());
    }

    // Returns an rvalue reference allowing code to use move semantics to 
    // extract data from the internal context. This means that the data stored 
    // in the context will be in a valid but unknown state.
    template <typename T>
    unqualified<T>&& extract(){ return std::any_cast<unqualified<T>&&>(ctx->a); }


    // returns true if atom has a value, else false
    inline bool operator bool() const { return !is_nil(); }
    inline bool operator==(const atom& b) const { return equalv(b); }
    inline bool operator==(atom&& b) const { return equalv(*this,b); }

private:
    struct context 
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

    mutable std::shared_ptr<context> ctx;

    context make_context(T&& t, std::true_type) const
    {
        auto f = to_fl_function(std::forward<T>(t));
        return std::make_shared<context>(std::move(f),compare_atom_function__<>);
    }

    context make_context(T&& t, std::false_type) const
    {
        return std::make_shared<context>(std::forward<T>(t),compare_any_function__<T>);
    }

    friend class print_map;
};

inline atom nil(){ return atom(); }
inline bool is_nil(atom a){ return a.is_nil(); }
template <typename T> bool is(atom a){ return a.is<T>(); }
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
template <typename T> inline atom copy(T& t){ return atom(std::forward<T>(t)); }
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



//-----------------------------------------------------------------------------
// atom printing 


namespace detail {
class print_map
{
public:
    print_map* instance();
    
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
        return type_map_[(get_std_type_name(a)].pr(a_ref);
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
// rebuilding a list via foldl will have the same effect with the extra
// functionality of operating on the data).
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
// sending atoms to a threadsafe circular queue.
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

    inline void make(size_t cap=1)
    {
        ctx = std::make_shared<channel_context>(cap);
    }

    inline void close(){ return ctx->close(); }
    inline bool closed(){ return ctx->closed(); }
    inline size_t size(){ return ctx->size(); }
    inline size_t capacity(){ return ctx->capacity(); }
    inline bool send(atom a){ return ctx->send(a); }
    inline bool recv(atom& a){ return ctx->recv(a); }

private:
    struct channel_context 
    {
        std::mutex mtx;
        std::condition_variable non_full_cv;
        std::condition_variable non_empty_cv;
        std::vector<atom> buf;
        const size_t cap;
        size_t head;
        size_t tail;
        size_t sz;
        bool closed;

        inline channel_context(size_t in_cap) : 
            cap(in_cap),
            head(0),
            tail(0),
            sz(0),
            closed(false)
        {
            if(!cap){ cap=1; }
            buf.reserve(cap);
        }

        inline void close()
        {
            std::unique_lock<std::mutex> lk(mtx);
            closed=true;
            non_full_cv.notify_all();
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
            return sz;
        }

        inline size_t capacity()
        {
            std::unique_lock<std::mutex> lk(mtx);
            return cap;
        }

        inline bool send(atom a)
        {
            atom ca = copy(a);
            std::unique_lock<std::mutex> lk(mtx);
            while(!closed && sz >= cap){ non_full_cv.wait(lk); }
            if(closed){ return false; }
            else 
            {
                buf[head] = ca;
                inc(head);
                ++sz;
                non_empty_cv.notify_one();
                return true;
            }
        }

        inline bool recv(atom& a)
        {
            std::unique_lock<std::mutex> lk(mtx);
            while(!closed && sz >= cap){ non_empty_cv.wait(lk); }
            if(closed){ return false; }
            else 
            {
                a = buf[tail];
                inc(tail);
                --sz;
                non_full_cv.notify_one();
                return true;
            }
        }

        inline void inc(size_t& i)
        {
            if(i+1 == cap){ i=0; }
            else{ ++i; }
        }
    };

    std::shared_ptr<channel_context> ctx;
};

channel make_channel()
{ 
    channel c;
    c.make();
    return c;
}



//-----------------------------------------------------------------------------
// thread worker

// worker is an interface to an std::thread stored in a std::shared_ptr.
//
// The std::thread will continually call its function f on atoms received from 
// the channel it was constructed with. This will continue until the worker's 
// context std::shared_ptr goes out of scope, upon which calls to fwill cease, 
// the worker function will return and the thread joined.
class worker 
{
public:
    inline worker(function f, channel in){ if(!ctx){ ctx = std::make_shared<worker_context>(c); } }
    inline worker(const worker& rhs) : ctx(rhs.ctx) { }
    inline worker(worker&& rhs) : ctx(std::move(rhs.ctx)) { }

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

    inline channel task_channel(){ return ctx->task_channel(); }

private:
    struct worker_context 
    {
    public:
        inline worker_context(channel in)
        {
            function f = [](atom a){ return eval(a); }
            start(f,in);
        }

        inline worker_context(function f, channel in)
        {
            ch = in;
            run = std::make_shared<bool>(false);
            done = std::make_shared<bool>(false);
            thd = std::thread([&](function f)
            {
                bool recv_success=false;
                std::unique_lock<std::mutex> lk(mtx);
                *run=true;
                ready_cv.notify_one();
                atom a;
                while(*run)
                {
                    lk.unlock();
                    success = ch.recv(a);
                    if(success)
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
                *done=true;
                done_cv.notify_one();
            }, f);

            std::unique_lock<std::mutex> lk(mtx);
            while(!*run){ ready_cv.wait(lk); }
        }

        inline channel task_channel(){ return ch; }

        inline ~worker_context()
        {
            {
                std::unique_lock<std::mutex> lk(mtx);
                *run = false;
                ch.close();
                while(!*done){ done_cv.wait(lk); }
            }
            thd.join();
        }

    private:
        std::mutex mtx;
        std::condition_variable ready_cv;
        std::condition_variable done_cv;
        std::thread thd;
        std::shared_ptr<bool> run;
        std::shared_ptr<bool> done;
        channel ch;
    };

    std::shared_ptr<worker_context> ctx;
};



//-----------------------------------------------------------------------------
// threadpool

class threadpool 
{
public:
    inline threadpool(){}
    inline threadpool(const threadpool& rhs) : ctx(rhs.ctx) { }
    inline threadpool(threadpool&& rhs) : ctx(std::move(rhs.ctx)) { }

    inline threadpool& operator=(const threadpool& rhs)
    {
        ctx = rhs.ctx;
        return *this;
    }

    inline threadpool& operator=(threadpool&& rhs)
    {
        ctx = std::move(rhs.ctx);
        return *this;
    }

    inline void start(size_t channel_size, 
                      size_t worker_count=std::thread::hardware_concurrency())
    {
        ctx = std::make_shared<threadpool_context>(channel_size, worker_count);
    }

    // channel to pass atoms to workers. If said atom is a list beginning with 
    // an fl::function said atom will be evaluated.
    inline channel task_channel(){ return ctx->task_channel(); }

private:
    struct threadpool_context 
    {
    public:
        threadpool_context(size_t worker_count, size_t channel_size) 
        { 
            if(!worker_count){ worker_count=1; }
            if(!channel_size){ channel_size=1; }

            function f = [](atom a){ return eval(a); };

            workers.reserve(worker_count);
            for(size_t i=0; i<worker_count; ++i)
            { 
                workers.emplace_back(f,make_channel(channel_size));
            }

            cur = workers.begin();
            end = workers.end();

            function f = [&](atom a)
            {
                cur->task_channel().send(a);
                if(cur==end){ cur = workers.begin(); }
                else{ ++cur; }
                return nil();
            }

            // distribution_worker is responsible for evenly scheduling atoms 
            // between all the workers
            ch.make(channel_size);
            distribution_worker = std::make_shared<worker>(f,ch);
        }

        channel task_channel(){ return ch; }

    private:
        channel ch;
        std::shared_ptr<worker> distribution_worker;
        std::vector<worker> workers;
        std::vector<worker>::iterator cur;
        std::vector<worker>::iterator end;
    };

    std::shared_ptr<threadpool_context> ctx;
};
} // end fl

#endif
