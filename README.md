# fl
## Rationale 
This project is a header only library which enables lisp-like functional programming in c++ providing the following functionality:

- generic datatype `atom`:
    - can accept any value
    - memory efficient by allowing const-like value re-use
    - enables by-value semantics for pointed to data 
    - various utilities for interacting with atoms with the following functions and templates:
        - `nil`
        - `is_nil`
        - `is`
        - `value`
        - `equalv`
        - `equalp`
        - `setv`
        - `mutable_value`
- lisp-list implementation 
    - `cons_cells` (with car/cdr), acting as a freeform tree-like structure 
    - ability to quote `atom`s to disable evaluation or unquote to re-enable 
    - ability to access or generate lists with the following functions:
        - `is_cons`
        - `cons`
        - `car`
        - `cdr`
        - `copy`
        - `inspect_list`
        - `is_list`
        - `length`
        - `head`
        - `tail`
        - `reverse`
        - `copy_list`
        - `append`
        - `quote`
        - `is_quoted`
        - `unquote`
- ability to stringify `atom`s and `list`s
    - can acquire a string representation of an `atom`s or `list`s stored type name(s) with:
        - `to_string`
- evaluation of `atom`s as code
    - arbitrary std::function/function pointer conversion to the `atom` datatype with function `atomize_function` which enables the following features for said functions:
        - ability to be `eval`uated
        - ability to curry said function into one that can accept arguments as a list
        - ability to automatically attempt to retrieve the expected value type from each atom based on the type of each argument in the original function. The exception to this behavior is if the function expects an atom for a given argument then an unmodified atom will be passed to it.
    - ability to evaluate an atom with function `eval`
- ability to iterate `list`s and apply functions to their data 
    - `map` can apply a function to one or more `list`s by index and construct another list of all the function results 
    - `foldl` can apply a function to one or more `list`s by index and return a single value
    - `foldr` can do the same as foldl but iterate in reverse order
    - `mapl` same as `map` but iterates over N entries instead of all entries
    - `foldll` same as `foldl` but iterates over N entries instead of all entries
    - `foldrl` same as `foldr` but iterates over N entries instead of all entries
- ability to convert the data in any forward iterable std:: container into a list of atoms with function `atomize_container`
- ability to convert a list of atoms into any size constructable std:: container with function `reconstitute_container`
- ability to iterate over a `list` using std:: compatible iterators:
    - `iterator`
    - `const_iterator`
    - `begin`
    - `end`
    - `cbegin`
    - `cend`
