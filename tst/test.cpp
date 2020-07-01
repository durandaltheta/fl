#include <gtest>

#include <vector>
#include <list>
#include <forward_list>
#include <map>

#include "fl.hpp"

using namespace fl;


//-----------------------------------------------------------------------------
// atom tests
TEST(atom,atom_default_constructor){}
TEST(atom,atom_lvalue_constructor){}
TEST(atom,atom_rvalue_constructor){}
TEST(atom,atom_T_constructor){}
TEST(atom,operator_bool){}
TEST(atom,nil){}
TEST(atom,is_nil){}


//-----------------------------------------------------------------------------
// type and value tests
TEST(type_and_value,is){}
TEST(type_and_value,lvalue_value){}
TEST(type_and_value,rvalue_value){}


//-----------------------------------------------------------------------------
// equality tests
TEST(equality,equalv){}
TEST(equality,equalp){}


//-----------------------------------------------------------------------------
// mutable tests
TEST(mutate,setv){}
TEST(mutate,mutable_value){}


//-----------------------------------------------------------------------------
// cons tests
TEST(cons,cons_cell_default_constructor){}
TEST(cons,cons_cell_lvalue_lhs_constructor){}
TEST(cons,cons_cell_lvalue_lhs_lvalue_rhs_constructor){}
TEST(cons,cons_cell_rvalue_lhs_constructor){}
TEST(cons,cons_cell_rvalue_lhs_rvalue_rhs_constructor){}
TEST(cons,cons_cell_lvalue_lhs_rvalue_rhs_constructor){}
TEST(cons,cons_cell_rvalue_lhs_lvalue_rhs_constructor){}
TEST(cons,cons_cell_operator_bool){}
TEST(cons,cons_cell_operator_copy_lvalue){}
TEST(cons,cons_cell_operator_copy_rvalue){}
TEST(cons,cons_cell_operator_equality_lvalue){}
TEST(cons,cons_cell_operator_equality_rvalue){}
TEST(cons,cons_cell_is_cons){}
TEST(cons,cons_cell_cons){}
TEST(cons,cons_cell_car){}
TEST(cons,cons_cell_cdr){}
TEST(cons,cons_cell_copy){}


//-----------------------------------------------------------------------------
// quote tests
TEST(quote,quote){}
TEST(quote,is_quote){}
TEST(quote,unquote){}


//-----------------------------------------------------------------------------
// list tests
TEST(list,inspect_list){}
TEST(list,is_list){}
TEST(list,length){}
TEST(list,head){}
TEST(list,tail){}
TEST(list,nth){}
TEST(list,reverse){}
TEST(list,copy_list){}
TEST(list,append){}


//-----------------------------------------------------------------------------
// print tests
TEST(print,fprint){}
TEST(print,print){}


//-----------------------------------------------------------------------------
// evaluation tests
TEST(evaluation,curry_std_function){}
TEST(evaluation,curry_function_pointer){}
TEST(evaluation,atomize_function_default_lvalue){}
TEST(evaluation,atomize_function_default_rvalue){}
TEST(evaluation,atomize_function_F){}
TEST(evaluation,eval){}


//-----------------------------------------------------------------------------
// iteration tests
TEST(iteration,map){}
TEST(iteration,mapl){}
TEST(iteration,foldl){}
TEST(iteration,foldll){}
TEST(iteration,foldr){}
TEST(iteration,foldrl){}
TEST(iteration,){}


//-----------------------------------------------------------------------------
// std:: container conversion tests
TEST(std_conversion,atomize_container){}
TEST(std_conversion,reconstitute_container){}


//-----------------------------------------------------------------------------
// std:: iteration of lists
TEST(std_iteration,iterator){}
TEST(std_iteration,const_iterator){}
TEST(std_iteration,begin){}
TEST(std_iteration,end){}
TEST(std_iteration,cbegin){}
TEST(std_iteration,cend){}
