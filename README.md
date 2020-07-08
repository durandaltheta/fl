# fl
## Table of Contents
- [Rationale](#Rationale)
- [API Core](#API-Core)
- [API Extended](#API-Extended)
- [API Concurrency](#API-concurrency)
- [API std](#API-std)
- [Example Programs(#Example-Programs)

## Rationale 
This project is a library which enables lisp-like functional programming in c++ providing the following functionality:

- generic datatype `fl::atom`:
    - can accept any value
    - memory efficient through implicit use of shared_ptr for the any value
    - enables by-value semantics for pointed to data 
    - memory efficient by allowing value re-use
    - various utilities for interacting with atoms.
- lisp-list implementation 
    - Implementation of cons_cells with `fl::cons()` (with `fl::car()`/`fl::cdr()`), acting as a freeform tree-like structure 
    - ability to `fl::quote` `fl::atom`s to disable evaluation or unquote to re-enable 
    - ability to access or generate lists.
- ability to stringify `fl::atom`s and lists
    - can acquire a string representation of an `fl::atom`s or lists stored type name(s) with:
        - `fl::to_string()`
- evaluation of `fl::atom`s as code
    - arbitrary, implicit std::function/function pointer conversion to the `atom` datatype (using function `fl::atomize_function()`) which enables the following features for said functions:
        - ability to `fl::curry()` said function into one that can accept arguments as a list
        - ability to automatically attempt to retrieve the expected value type from each atom based on the type of each argument in the original function. The exception to this behavior is if the function expects an atom for a given argument then an unmodified atom will be passed to it.
        - ability to be executed in function `fl::eval()`
- ability to iterate `list`s and apply functions to their data using a variety of algorithms including `fl::map()` and `fl::foldl()`
- ability to convert the data in any forward iterable std:: container into a list of atoms with function `fl::atomize_container()`
- ability to convert a list of `fl::atom`s into any size constructable std:: container with function `fl::reconstitute_container()`
- ability to iterate over `fl::atom` lists using std:: compatible iterators
- ability to communicate `fl::atom`s between threads using the threadsafe `fl::channel`
- ability to launch `fl::worker` threads and/or groups of worker threads
  (`fl::workerpool`s) capable of `fl::eval()`uating atoms sent to it.
- ability to generically `fl::schedule()` atoms on a `fl::worker`/`fl::workerpool` 
- ability to implement non-blocking communication & scheduling with `fl::continuation`

## API core
### fl::atom 
### fl::function
### fl::cons(), fl::car(), fl::cdr() 
## API extended
### list construction and interaction
#### fl::list()
#### fl::inspect_list()
#### fl::is_list()
#### fl::length()
#### fl::head_cons()
#### fl::tail_cons()
#### fl::nth_cons()
#### fl::head()
#### fl::tail()
#### fl::nth()
#### fl::reverse()
### evaluation
### list iteration
### to_string()
## API concurrency
### fl::channel
### fl::worker
### fl::workerpool
### fl::continuation
### fl::io()
## API std:: 


## Example programs
``` 
//hello
#include <iostream>
#include "fl.hpp"

int main()
{
    fl::atom lst = fl::list("hello"," ","world");
    auto print = [](const std::string& s){ std::cout << s; };
    fl::for_each(print, lst);
    std::cout << std::endl;

    std::cout << fl::to_string(lst) << std::endl;

    fl::atom combined = fl::append(fl::list(print), lst);
    std::cout << fl::to_string(combined) << std::endl;
    return 0;
}
```
```
$ ./hello 
hello world 
("hello" " " "world")
(lambda(const std::string& s):0x0000000000 "hello" " " "world")
$
```
