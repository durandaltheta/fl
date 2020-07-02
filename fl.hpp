#ifndef __CPP_FUNCTIONAL_LISPLIKE__
#define __CPP_FUNCTIONAL_LISPLIKE__

// c++
#include <memory>
#include <optional>
#include <any>
#include <typeinfo>
#include <type_traits>
#include <iostream>
#include <mutex>

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

struct atom; // forward declaration

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
                    return fun(value<T&>(a));
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
                [=](Ts&&...ts){ return fun(value<T&>(a), ts...); }
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


// atomize_function will take any std::function object and return an atom which 
// can be executed in eval()

namespace detail {
atom eval_curried_function(const function& f, atom a){ return f(a); }
atom eval_curried_function(function&& f, atom a){ return f(a); }

// recursive variation
template <typename F>
atom eval_curried_function(F&& f, atom a){ return eval_curried_function(f(car(a)),cdr(a)); }
}

// atomize_function converts any callable to an fl::function
template <typename F>
function to_fl_function(F&& f)
{
    function af = [=](atom a) -> atom
    {
        auto std_f = to_std_function(std::forward<F>(f));
        auto curried_f = curry(std_f);
        return detail::eval_curried_function(curried_f,a); 
    };
    return af;
}



//-----------------------------------------------------------------------------
// atom 

template <typename T> class register_type; 
#define REGISTER_TYPE__(T) register_type<T>(#T)

struct atom 
{
    std::shared_ptr<std::optional<std::any>> a;

    atom(){}
    atom(const atom& rhs) : a(rhs.a) {}
    atom(atom&& rhs) : a(std::move(rhs.a)) {}

    template <typename T>
    atom(T&& t)
    {
        static REGISTER_TYPE__(T);
        make_shared_T(std::forward<T>(t),typename std::is_function<T>::type>
    }

    void make_shared_T(T&& t, std::true_type)
    {
        auto f = to_fl_function(std::forward<T>(t));
        a = std::make_shared<std::optional<std::any>>(std::move(f));
    }

    void make_shared_T(T&& t, std::false_type)
    {
        a = std::make_shared<std::optional<std::any>>(std::forward<T>(t));
    }

    // returns true if atom has a value, else false
    bool operator bool()
    {
        // std::shared_ptr and std::optional are valid and std::any has a value
        if(a && *a && (**a).has_value()){ return true; }
        else{ return false; }
    }
};

inline atom nil(){ return atom(); }
inline bool is_nil(atom a) const { return a ? false : true; }



//-----------------------------------------------------------------------------
// type and value

// returns nil if value is not the specified type, else a valid atom
template <typename T>
bool is(atom a)
{ 
    atom b(T());
    if(a && (*(a.a))->type() == (*(b.a))->type()){ return atom(true); }
    else{ return atom(); }
}

// get atom's value
template <typename T>
const T& value(const atom& a) const { return std::any_cast<const T&>(**(a.a)); }

template <typename T>
T value(atom&& a) { return std::any_cast<T&&>(**(a.a)); }



//-----------------------------------------------------------------------------
// comparison  

// compare atoms, returns true if both atomic values of the specified type are 
// equal with == operator
template <typename T>
bool equalv(atom lhs, atom rhs)
{
    return value<T>(lhs) == value<T>(rhs);
}

// pointer (pointer managed by std::shared_ptr) equality
inline bool equalp(atom lhs, atom rhs)
{
    return lhs.a.get() == rhs.a.get();
}



//-----------------------------------------------------------------------------
// mutable operations 

// can be dangerous to mutate values, this should be generally avoided

// forcibly set value of atom 
template <typename T>
atom setv(atom a, T&& t)
{ 
    **(a.a) = std::forward<T>(t); 
    return a;
}

// return a mutable reference to the value of an atom
template <typename T>
T& mutable_value(atom& a) const { return std::any_cast<T&>(**(a.a)); }



//-----------------------------------------------------------------------------
// cons_cell 

// forward declarations
bool is_cons(atom a) const; 

namespace detail {
class cons_cell 
{
public:
    atom car;
    atom cdr;

    cons_cell(){}
    cons_cell(const atom& a) : car(a) {}
    cons_cell(const atom& a, const atom& b) : car(a), cdr(b) {}
    cons_cell(atom&& a) : car(std::move(a)) {}
    cons_cell(atom&& a, atom&& b) : car(std::move(a)), cdr(std::move(b)) {}
    cons_cell(const atom& a, atom&& b) : car(a), cdr(std::move(b)) {}
    cons_cell(atom&& a, const atom& b) : car(std::move(a)), cdr(b) {}

    // returns true if cons_cell is nil, else false
    bool operator bool(){ return a.car || a.cdr; }

    cons_cell& operator=(const cons_cell& rhs)
    {
        car = rhs.car;
        cdr = rhs.cdr;
        return *this;
    }

    cons_cell& operator=(cons_cell&& rhs)
    {
        car = std::move(rhs.car);
        cdr = std::move(rhs.cdr);
        return *this;
    }

    bool operator==(const cons_cell& rhs)
    {
        return compare(car,rhs.car) && compare(cdr,rhs.cdr);
    }

    bool operator==(cons_cell&& rhs){ return *this == rhs; }
};
}

template <typename A, typename B>
inline atom cons(A&& a, B&& b)
{ 
    return atom(detail::cons_cell(atom(std::forward<A>(a)), 
                                  atom(std::forward<B>(b)))); 
}

inline bool is_cons(atom a) const { return is<detail::cons_cell>(a) ? true : false; }
inline atom car(atom a){ return value<detail::cons_cell>(a).car; }
inline atom cdr(atom a){ return value<detail::cons_cell>(a).cdr; }

inline atom copy(atom a)
{ 
    atom b;
    *(b.a) = *(a.a.);
    return b;
}



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

inline atom head(atom lst){ return car(lst); }
inline atom tail(atom lst)
{ 
    if(is_nil(lst)){ return lst; }
    else 
    {
        while(!is_nil(cdr(lst))){ lst = cdr(lst); }
        return car(lst);
    }
}

inline atom nth(atom lst, size_t n)
{
    while(n)
    {
        --n;
        lst = cdr(lst);
    }
    return car(lst);
}

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

namespace detail {
struct meta_list
{
    atom head;
    atom tail;
    size_t sz;
};

inline meta_list copy_list(atom lst)
{
    atom ret; // nil
    atom cur = ret;
    size_t sz=0;
    if(is_nil(lst)){ return meta_list(ret,cur,0); } 
    else 
    {
        atom ret = cons(copy(car(lst)), copy(cdr(lst)));
        atom cur = cdr(ret);
        lst = cdr(lst);
        ++sz;
        while(!is_nil(lst))
        {
            cur = cons(copy(car(lst)), copy(cdr(lst)));
            cur = cdr(cur);
            lst = cdr(lst);
            ++sz
        }
        copy(cdr(cur),nil());
        return meta_list(ret,cur,sz);
    }
}
}

inline atom copy_list(atom lst)
{
    return detail::copy_list(lst).head;
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



//-----------------------------------------------------------------------------
// quote
namespace detail {
class quote{};
}

inline atom quote(atom a){ return cons(atom(detail::quote()),a); }
inline bool is_quoted(atom a){ return is<detail::quote>(a); }

inline atom unquote(atom a)
{
    if(is_quoted(car(a))
    {
        do{ a = cdr(a); } while(is_quoted(car(a)));
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
template <typename T>
using unqualified = typename std::decay<T>::value;


template <typename T> 
class register_type;

class print_map
{
public:
    print_map* instance();
    inline std::string type(atom a)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        return map_[a.type().name()];
    }

private:
    std::mutex mtx_;
    std::map<std::string,const char*> map_;

    template <typename T> 
    void register_type(const char* name)
    { 
        T t;
        std::any a(t);
        std::unique_lock<std::mutex> lk(mtx_); 
        map_[a.type().name()] = name;
    } 

    friend template <typename T> class register_type;
}

template <typename T>
class register_type
{
public:
    register_type(const char* name)
    {
        detail::print_map::instance()->register_type<unqualified<T>>(name);
    }
};
}

inline const char* atom_type_name(atom a){ return detail::print_map::instance()->type(a); }


namespace detail {
template <typename STREAM>
inline std::string to_string(atom a);

template <typename STREAM>
inline std::string to_string1(atom a)
{
    if(is_cons(a)){ return to_string(a); }
    else if(is_quoted(a)){ return std::string("'"); }
    else if(is_nil(a)){ return std::string("nil"); }
    else{ return atom_type_name(a); }
}

inline std::string to_string(atom a)
{ 
    std::string s;
    if(is_cons(a))
    {
        if(is_quoted(car(a))){ s+=std::string("'("); }
        else 
        {
            s+=std::string("("); 
            s+=to_string1(car(a));
        }

        a = cdr(a);

        while(!is_nil(a))
        {
            s+=std::string(" "); 
            s+=to_string1(a);
            a = cdr(a);
        }
        s+=std::string(")");
    }
    else{ s+=to_string1(a); }
    return s;
}
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

#endif
