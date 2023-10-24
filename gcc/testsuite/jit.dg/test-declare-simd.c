/* { dg-do compile { target x86_64-*-* } } */

#include <stdlib.h>
#include <stdio.h>

#include "libgccjit.h"

/* Manually set -O3 to see the effect of omp declare simd attribute */
#define TEST_ESCHEWS_SET_OPTIONS
static void set_options (gcc_jit_context *ctxt, const char *argv0)
{
	// Set "-O3".
	gcc_jit_context_set_int_option(ctxt, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 3);
    gcc_jit_context_add_command_line_option(ctxt, "-ffast-math");
    gcc_jit_context_add_command_line_option(ctxt, "-ftree-loop-vectorize");   
    gcc_jit_context_add_command_line_option(ctxt, "-march=x86-64-v2");
}

#define TEST_COMPILING_TO_FILE
#define OUTPUT_KIND      GCC_JIT_OUTPUT_KIND_ASSEMBLER
#define OUTPUT_FILENAME  "output-of-test-declare-simd.c.s"
#include "harness.h"

void
create_code (gcc_jit_context *ctxt, void *user_data)
{
	/* Let's try to inject the equivalent of:
void test(double *__restrict__ a, double *__restrict__ b, int n) {
	int i;
	i = 0;
	while (i < n) {
		a[i] = cos(b[i]);
		i = i + 1;
	}
}
	*/

	gcc_jit_function *cos =
		gcc_jit_context_get_builtin_function(ctxt, "cos");
	gcc_jit_function_set_bool_declare_simd(cos, 1);

	gcc_jit_type *int_type =
		gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_INT);
	gcc_jit_type *void_type =
		gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_VOID);
	gcc_jit_type *double_type =
		gcc_jit_context_get_type (ctxt, GCC_JIT_TYPE_DOUBLE);
	gcc_jit_type *p_double_type = 
		gcc_jit_type_get_pointer(double_type);
	gcc_jit_type *p_double_restrict_type =
		gcc_jit_type_get_restrict(p_double_type);

	gcc_jit_param *a =
		gcc_jit_context_new_param (ctxt, NULL, p_double_restrict_type, "a");
	gcc_jit_param *b =
		gcc_jit_context_new_param (ctxt, NULL, p_double_restrict_type, "b");
	gcc_jit_param *n =
		gcc_jit_context_new_param (ctxt, NULL, int_type, "n");
	gcc_jit_param *params[3] = {a, b, n};

	gcc_jit_function *func_t =
		gcc_jit_context_new_function (ctxt, NULL,
					GCC_JIT_FUNCTION_EXPORTED,
					void_type,
					"test",
					3, params,
					0);

	gcc_jit_block *block = gcc_jit_function_new_block (func_t, NULL);

	gcc_jit_lvalue *i =
		gcc_jit_function_new_local(func_t, NULL, int_type, "i");

	/* i = 0; */
	gcc_jit_block_add_assignment(
		block, NULL, i, 
		gcc_jit_context_zero(ctxt, int_type));

	
	gcc_jit_block *header = gcc_jit_function_new_block(func_t, "header");
	gcc_jit_block *body = gcc_jit_function_new_block(func_t, "body");
	gcc_jit_block *exit = gcc_jit_function_new_block(func_t, "exit");
	gcc_jit_block_end_with_jump(block, NULL, header);

	/* while (i < n) { */
	gcc_jit_rvalue *less_than =
		gcc_jit_context_new_comparison(
			ctxt, NULL, GCC_JIT_COMPARISON_LT,
			gcc_jit_lvalue_as_rvalue(i),
			gcc_jit_param_as_rvalue(n));
	gcc_jit_block_end_with_conditional(header, NULL, less_than, body, exit);

	/* a[i] = cos(b[i]); */
	gcc_jit_lvalue *ai = 
		gcc_jit_context_new_array_access(
			ctxt, NULL,
			gcc_jit_param_as_rvalue(a),
			gcc_jit_lvalue_as_rvalue(i));
        gcc_jit_rvalue *bi =
            gcc_jit_lvalue_as_rvalue(
		gcc_jit_context_new_array_access(
			ctxt, NULL,
			gcc_jit_param_as_rvalue(b),
			gcc_jit_lvalue_as_rvalue(i)));
	gcc_jit_rvalue *cos_bi =
		gcc_jit_context_new_call(
			ctxt, NULL,
			cos, 1, &bi);
	gcc_jit_block_add_assignment(body, NULL, ai, cos_bi);

	/* i = i + 1; */
	gcc_jit_block_add_assignment_op(
		body, NULL, i,
		GCC_JIT_BINARY_OP_PLUS,
		gcc_jit_context_one(ctxt, int_type));
	gcc_jit_block_end_with_jump(body, NULL, header);
	gcc_jit_block_end_with_void_return (exit, NULL);
}

/* { dg-final { jit-verify-output-file-was-created "" } } */
/* { dg-final { jit-verify-assembler-output "_ZGVbN2v_cos" } } */
