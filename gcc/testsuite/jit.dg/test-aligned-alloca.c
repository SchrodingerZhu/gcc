#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "libgccjit.h"

#include "harness.h"

void
fill (void *ptr) {
    for (int i = 0; i < 100; i++)
        ((int *)ptr)[i] = i;
}

void
sum (void *ptr, int *sum) {
    *sum = 0;
    for (int i = 0; i < 100; i++)
        *sum += ((int *)ptr)[i];
}


void
create_code (gcc_jit_context *ctxt, void *user_data)
{
    /*
    bool test_aligned_alloca (typeof(fill) *fill, typeof(sum) *sum, int* sum_p) {
        size_t addr;
        void *p;

        p = __builtin_alloca_with_align (sizeof (int) * 100, 128);
        addr = (size_t)p;
        fill (p);
        sum (p, sum_p);
        return (addr & 127) == 0;
    }
    */

    /* Types */
    gcc_jit_type *size_t_type 
        = gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *int_type 
        = gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_INT);
    gcc_jit_type *int_ptr_type 
        = gcc_jit_type_get_pointer (int_type);
    gcc_jit_type *void_ptr_type 
        = gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *bool_type
        = gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_BOOL);
    gcc_jit_type *void_type
        = gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_VOID);
    gcc_jit_type *fill_ptr_type
        = gcc_jit_context_new_function_ptr_type (ctxt, NULL, void_type, 1, &void_ptr_type, 0);
    gcc_jit_type *sum_params[] = {void_ptr_type, int_ptr_type};
    gcc_jit_type *sum_ptr_type
        = gcc_jit_context_new_function_ptr_type (ctxt, NULL, void_type, 2, sum_params, 0);

    /* Function */
    gcc_jit_param *fill_param
        = gcc_jit_context_new_param (ctxt, NULL, fill_ptr_type, "fill");
    gcc_jit_rvalue *rv_fill = gcc_jit_param_as_rvalue (fill_param);
    gcc_jit_param *sum_param
        = gcc_jit_context_new_param (ctxt, NULL, sum_ptr_type, "sum");
    gcc_jit_rvalue *rv_sum = gcc_jit_param_as_rvalue (sum_param);
    gcc_jit_param *sum_p_param
        = gcc_jit_context_new_param (ctxt, NULL, int_ptr_type, "sum_p");
    gcc_jit_rvalue *rv_sum_p = gcc_jit_param_as_rvalue (sum_p_param);
    gcc_jit_param *params[] = {fill_param, sum_param, sum_p_param};
    gcc_jit_function *test_aligned_alloca
        = gcc_jit_context_new_function (ctxt, NULL, GCC_JIT_FUNCTION_EXPORTED, bool_type, "test_aligned_alloca", 3, params, 0);
    

    /* Variables */
    gcc_jit_lvalue *addr 
        = gcc_jit_function_new_local (test_aligned_alloca, NULL, size_t_type, "addr");
    gcc_jit_lvalue *p 
        = gcc_jit_function_new_local (test_aligned_alloca, NULL, void_ptr_type, "p");
    gcc_jit_rvalue *rv_addr = gcc_jit_lvalue_as_rvalue (addr);
    gcc_jit_rvalue *rv_p = gcc_jit_lvalue_as_rvalue (p);


    /* Blocks */
    gcc_jit_block *block 
        = gcc_jit_function_new_block (test_aligned_alloca, NULL);

    /* p = __builtin_alloca_with_align (sizeof (int) * 100, 128); */
    gcc_jit_rvalue *sizeof_int 
        = gcc_jit_context_new_sizeof(ctxt, int_type);
    gcc_jit_rvalue *c100 
        = gcc_jit_context_new_rvalue_from_int(ctxt, int_type, 100);
    gcc_jit_rvalue *size 
        = gcc_jit_context_new_binary_op(ctxt, NULL, GCC_JIT_BINARY_OP_MULT, size_t_type, sizeof_int, c100);
    gcc_jit_rvalue *c128
        = gcc_jit_context_new_rvalue_from_int(ctxt, size_t_type, 128);
    gcc_jit_function *alloca_with_align
        = gcc_jit_context_get_builtin_function (ctxt, "__builtin_alloca_with_align");
    gcc_jit_rvalue *args[] = {size, c128};
    gcc_jit_rvalue *alloca_with_align_call
        = gcc_jit_context_new_call (ctxt, NULL, alloca_with_align, 2, args);
    gcc_jit_block_add_assignment (block, NULL, p, alloca_with_align_call);

    /* addr = (size_t)p; */
    gcc_jit_rvalue *cast_p
        = gcc_jit_context_new_bitcast (ctxt, NULL, rv_p, size_t_type);
    gcc_jit_block_add_assignment (block, NULL, addr, cast_p);

    /* fill (p); */
    gcc_jit_rvalue * call_fill = gcc_jit_context_new_call_through_ptr(
        ctxt, NULL, rv_fill, 1, &rv_p);
    gcc_jit_block_add_eval (block, NULL, call_fill);

    /* sum (p, sum_p); */
    gcc_jit_rvalue * sum_args[] = {rv_p, rv_sum_p};
    gcc_jit_rvalue * call_sum = gcc_jit_context_new_call_through_ptr(
        ctxt, NULL, rv_sum, 2, sum_args);
    gcc_jit_block_add_eval (block, NULL, call_sum);

    /* return (addr & 127) == 0; */
    gcc_jit_rvalue *c127
        = gcc_jit_context_new_rvalue_from_int(ctxt, size_t_type, 127);
    gcc_jit_rvalue *and_op
        = gcc_jit_context_new_binary_op(ctxt, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, size_t_type, rv_addr, c127);
    gcc_jit_rvalue *c0
        = gcc_jit_context_new_rvalue_from_int(ctxt, size_t_type, 0);
    gcc_jit_rvalue *eq_op
        = gcc_jit_context_new_comparison(ctxt, NULL, GCC_JIT_COMPARISON_EQ, and_op, c0);
    gcc_jit_block_end_with_return (block, NULL, eq_op);
}

void
verify_code (gcc_jit_context *ctxt, gcc_jit_result *result)
{
  typedef void (*fn_type) (typeof(fill) *, typeof(sum) *, int *);
  CHECK_NON_NULL (result);
  fn_type test_aligned_alloca =
    (fn_type)gcc_jit_result_get_code (result, "test_aligned_alloca");
  CHECK_NON_NULL (test_aligned_alloca);
  int value;
  test_aligned_alloca (fill, sum, &value);
  CHECK (value == 4950);
}
