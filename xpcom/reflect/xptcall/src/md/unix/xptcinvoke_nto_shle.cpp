/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xptcprivate.h"

extern "C" {

const int c_int_register_params = 3;
const int c_float_register_params = 8;

static void
copy_to_stack(uint32_t paramCount, nsXPTCVariant* s, int size, uint32_t* data)
{
	int intCount = 0;
	int floatCount = 0;
	uint32_t *intRegParams = data + (size / sizeof(uint32_t)) - c_int_register_params;
	float  *floatRegParams = (float *)intRegParams - c_float_register_params;

    for ( uint32_t i = 0; i < paramCount; ++i, ++s )
    {
		nsXPTType type = s->IsPtrData() ? nsXPTType::T_I32 : s->type;

		switch ( type ) {
        case nsXPTType::T_I64:
        case nsXPTType::T_U64:
			// Space to pass in registers?
			if ( (c_int_register_params - intCount) >= 2 ) {
				*((int64_t *) intRegParams) = s->val.i64;
				intRegParams += 2;
				intCount += 2;
			}
			else {
				*((int64_t*) data) = s->val.i64;
				data += 2;
			}
			break;
        case nsXPTType::T_FLOAT:
			// Space to pass in registers?
			if ( floatCount < c_float_register_params ) {
				*floatRegParams = s->val.f;
				++floatCount;
				++floatRegParams;
			}
			else {
				*((float*) data) = s->val.f;
				++data;
			}			
			break;
        case nsXPTType::T_DOUBLE:
			// Space to pass in registers?
			if ( (c_float_register_params - floatCount) >= 2  ) {
				if ( (floatCount & 1) != 0 ) {
					++floatCount;
					++floatRegParams;					
				}
				*(double *)floatRegParams = s->val.d;
				floatCount += 2;
				floatRegParams += 2;
			}
			else {
				*((double *) data) = s->val.d;
				data += 2;
			}			
			break;
		default:		// 32 (non-float) value
			int32_t value = (int32_t) (s->IsPtrData() ?  s->ptr : s->val.p);
			// Space to pass in registers?
			if ( intCount < c_int_register_params ) {
				*intRegParams = value;
				++intRegParams;
				++intCount;
			}
			else {
				*data = value;
				++data;
			}
			break;
        }
    }	
}

XPTC_PUBLIC_API(nsresult)
XPTC_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                   uint32_t paramCount, nsXPTCVariant* params)
{
#ifdef __GNUC__            /* Gnu compiler. */
	int result;
	void (*fn_copy) (uint32_t paramCount, nsXPTCVariant* s, int size, uint32_t* d) = copy_to_stack;

	/* Because the SH processor passes the first few parameters in registers
	   it is a bit tricky setting things up right.  To make things easier,
	   all the hard work will be done by copy_to_stack above.  We pass to it
	   a chunk of memory, the top of which will be copied to registers r5 to r7
	   and fr4 to fr11 before calling the target function.
	*/

 __asm__ __volatile__(
    /* Make space for parameters to be passed to the method.  Assume worst case 
	   8 bytes per parameter.  Also leave space for 3 longs and 8 floats that
	   will be put into registers.  The worst case is all int64_t parameters
	   and even in this case 8 bytes are passed in registers so we can
	   deduct this from our allocation.
	*/
	"mov.l	@(8,r14), r4 \n\t"	// copy_to_stack paramCount parameter
	"mov	r4, r6 \n\t"
	"mov	#3, r1 \n\t"
	"shld	r1, r6 \n\t"
	"add	#36, r6 \n\t"		// Space for 3 longs, 8 floats - 8 bytes
	"sub	r6, r15 \n\t"
	"mov	r15, r7 \n\t"
	"mov.l	@(20,r14), r1 \n\t"	// fn_copy
	"jsr	@r1 \n\t"			// Note, next instruction is executed first
	"mov.l	@(12,r14), r5 \n\t"	// copy_to_stack data parameter

	/* Now everything is laid out nicely in the stack.  We just have to
	   load values at the top end of the memory area into registers and
	   make the call.  We may load more things into registers than needed,
	   but nobody will care about that.
	*/	
	"mov	r14, r1 \n\t"
	"add	#-44, r1 \n\t"
	"fmov.s	@r1+, fr5 \n\t"
	"fmov.s	@r1+, fr4 \n\t"
	"fmov.s	@r1+, fr7 \n\t"
	"fmov.s	@r1+, fr6 \n\t"
	"fmov.s	@r1+, fr9 \n\t"
	"fmov.s	@r1+, fr8 \n\t"
	"fmov.s	@r1+, fr11 \n\t"
	"fmov.s	@r1+, fr10 \n\t"
	"mov.l	@r1+, r5 \n\t"
	"mov.l	@r1+, r6 \n\t"
	"mov.l	@r1+, r7 \n\t"	
	"mov.l	@r14, r1 \n\t"		// that -> r1
	"mov.l	@r1, r1 \n\t"		// vtbl -> r1
	"mov.l	@(4,r14), r4 \n\t"	// methodIndex -> r4
	"shll2	r4	\n\t"
	"add	r4, r1 \n\t"
	"mov.l	@r1, r1 \n\t"		// method addr -> r1
	"jsr	@r1 \n\t"
	"mov.l	@r14, r4 \n\t"		// that -> r4  (note, executed before jsr)
	"mov.l	r0, @(16,r14) \n\r"	// Save return value in result
    : : : "r1", "r4", "r6", "r7", "pr", "dr4", "dr6", "dr8"
   );
    
  return result;

#else
#error "can't find a compiler to use"
#endif /* __GNUC__ */

}

}
