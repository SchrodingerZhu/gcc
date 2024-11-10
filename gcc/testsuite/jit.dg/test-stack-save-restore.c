#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "libgccjit.h"

#include "harness.h"

void
create_code (gcc_jit_context *ctxt, void *user_data)
{
  /*
  size_t test_stack_save_restore() {
      void *p;
      size_t a, b;
      p = __builtin_stack_save();
      a = (size_t)__builtin_alloca(1024);
      __builtin_stack_restore(p);

      p = __builtin_stack_save();
      b = (size_t)__builtin_alloca(512);
      __builtin_stack_restore(p);

      return b - a;
      }
  */

  /* Types */
  gcc_jit_type *size_t_type
      = gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_SIZE_T);
  gcc_jit_type *void_ptr_type
      = gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_VOID_PTR);

  /* Function */
  gcc_jit_function *test_stack_save_restore = gcc_jit_context_new_function (
      ctxt, NULL, GCC_JIT_FUNCTION_EXPORTED, size_t_type,
      "test_stack_save_restore", 0, NULL, 0);

  /* Variables */
  gcc_jit_lvalue *p = gcc_jit_function_new_local (test_stack_save_restore,
                                                  NULL, void_ptr_type, "p");
  gcc_jit_lvalue *a = gcc_jit_function_new_local (test_stack_save_restore,
                                                  NULL, size_t_type, "a");
  gcc_jit_lvalue *b = gcc_jit_function_new_local (test_stack_save_restore,
                                                  NULL, size_t_type, "b");
  gcc_jit_rvalue *rv_p = gcc_jit_lvalue_as_rvalue (p);
  gcc_jit_rvalue *rv_a = gcc_jit_lvalue_as_rvalue (a);
  gcc_jit_rvalue *rv_b = gcc_jit_lvalue_as_rvalue (b);

  /* Blocks */
  gcc_jit_block *block
      = gcc_jit_function_new_block (test_stack_save_restore, NULL);

  /* Builtin functions */
  gcc_jit_function *stack_save
      = gcc_jit_context_get_builtin_function (ctxt, "__builtin_stack_save");
  gcc_jit_function *stack_restore
      = gcc_jit_context_get_builtin_function (ctxt, "__builtin_stack_restore");
  gcc_jit_function *alloca
      = gcc_jit_context_get_builtin_function (ctxt, "__builtin_alloca");

  /* Common code */
  gcc_jit_rvalue *call_stack_save
      = gcc_jit_context_new_call (ctxt, NULL, stack_save, 0, NULL);
  gcc_jit_rvalue *call_stack_restore
      = gcc_jit_context_new_call (ctxt, NULL, stack_restore, 1, &rv_p);

  /* p = __builtin_stack_save(); */
  gcc_jit_block_add_assignment (block, NULL, p, call_stack_save);

  /* a = (size_t)__builtin_alloca(1024); */
  gcc_jit_rvalue *c1024
      = gcc_jit_context_new_rvalue_from_int (ctxt, size_t_type, 1024);
  gcc_jit_rvalue *call_alloca_1024
      = gcc_jit_context_new_call (ctxt, NULL, alloca, 1, &c1024);
  gcc_jit_rvalue *cast_alloca_1024 = gcc_jit_context_new_bitcast (
      ctxt, NULL, call_alloca_1024, size_t_type);
  gcc_jit_block_add_assignment (block, NULL, a, cast_alloca_1024);

  /* __builtin_stack_restore(p); */
  gcc_jit_block_add_eval (block, NULL, call_stack_restore);

  /* p = __builtin_stack_save(); */
  gcc_jit_block_add_assignment (block, NULL, p, call_stack_save);

  /* b = (size_t)__builtin_alloca(512); */
  gcc_jit_rvalue *c512
      = gcc_jit_context_new_rvalue_from_int (ctxt, size_t_type, 512);
  gcc_jit_rvalue *call_alloca_512
      = gcc_jit_context_new_call (ctxt, NULL, alloca, 1, &c512);
  gcc_jit_rvalue *cast_alloca_512
      = gcc_jit_context_new_bitcast (ctxt, NULL, call_alloca_512, size_t_type);
  gcc_jit_block_add_assignment (block, NULL, b, cast_alloca_512);

  /* __builtin_stack_restore(p); */
  gcc_jit_block_add_eval (block, NULL, call_stack_restore);

  /* return b - a; */
  gcc_jit_rvalue *sub = gcc_jit_context_new_binary_op (
      ctxt, NULL, GCC_JIT_BINARY_OP_MINUS, size_t_type, rv_b, rv_a);
  gcc_jit_block_end_with_return (block, NULL, sub);
}

void
verify_code (gcc_jit_context *ctxt, gcc_jit_result *result)
{
  typedef size_t (*fn_type) (void);
  CHECK_NON_NULL (result);
  fn_type test_stack_save_restore
      = (fn_type)gcc_jit_result_get_code (result, "test_stack_save_restore");
  CHECK_NON_NULL (test_stack_save_restore);
  size_t value = test_stack_save_restore ();
  CHECK_VALUE (value, 512 - 1024);
}
