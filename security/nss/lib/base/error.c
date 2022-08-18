/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * error.c
 *
 * This file contains the code implementing the per-thread error
 * stacks upon which most NSS routines report their errors.
 */

#ifndef BASE_H
#include "base.h"
#endif              /* BASE_H */
#include <limits.h> /* for UINT_MAX */
#include <string.h> /* for memmove */

#if defined(__MINGW32__)
#include <windows.h>
#endif

#define NSS_MAX_ERROR_STACK_COUNT 16 /* error codes */

/*
 * The stack itself has a header, and a sequence of integers.
 * The header records the amount of space (as measured in stack
 * slots) already allocated for the stack, and the count of the
 * number of records currently being used.
 */

struct stack_header_str {
    PRUint16 space;
    PRUint16 count;
};

struct error_stack_str {
    struct stack_header_str header;
    PRInt32 stack[1];
};
typedef struct error_stack_str error_stack;

/*
 * error_stack_index
 *
 * Thread-private data must be indexed.  This is that index.
 * See PR_NewThreadPrivateIndex for more information.
 *
 * Thread-private data indexes are in the range [0, 127].
 */

#define INVALID_TPD_INDEX UINT_MAX
static PRUintn error_stack_index = INVALID_TPD_INDEX;

/*
 * call_once
 *
 * The thread-private index must be obtained (once!) at runtime.
 * This block is used for that one-time call.
 */

static PRCallOnceType error_call_once;
static const PRCallOnceType error_call_again;

/*
 * error_once_function
 *
 * This is the once-called callback.
 */
static PRStatus
error_once_function(void)
{

/*
 * This #ifdef function is redundant. It performs the same thing as the
 * else case.
 *
 * However, the MinGW version looks up the function from nss3's export
 * table, and on MinGW _that_ behaves differently than passing a
 * function pointer in a different module because MinGW has
 * -mnop-fun-dllimport specified, which generates function thunks for
 * cross-module calls. And when a module (like nssckbi) gets unloaded,
 * and you try to call into that thunk (which is now missing) you'll
 * crash. So we do this bit of ugly to avoid that crash. Fortunately
 * this is the only place we've had to do this.
 */
#if defined(__MINGW32__)
    HMODULE nss3 = GetModuleHandleW(L"nss3");
    if (nss3) {
        PRThreadPrivateDTOR freePtr = (PRThreadPrivateDTOR)GetProcAddress(nss3, "PR_Free");
        if (freePtr) {
            return PR_NewThreadPrivateIndex(&error_stack_index, freePtr);
        }
    }
    return PR_NewThreadPrivateIndex(&error_stack_index, PR_Free);
#else
    return PR_NewThreadPrivateIndex(&error_stack_index, PR_Free);
#endif
}

/*
 * error_get_my_stack
 *
 * This routine returns the calling thread's error stack, creating
 * it if necessary.  It may return NULL upon error, which implicitly
 * means that it ran out of memory.
 */

static error_stack *
error_get_my_stack(void)
{
    PRStatus st;
    error_stack *rv;
    PRUintn new_size;
    PRUint32 new_bytes;
    error_stack *new_stack;

    if (INVALID_TPD_INDEX == error_stack_index) {
        st = PR_CallOnce(&error_call_once, error_once_function);
        if (PR_SUCCESS != st) {
            return (error_stack *)NULL;
        }
    }

    rv = (error_stack *)PR_GetThreadPrivate(error_stack_index);
    if ((error_stack *)NULL == rv) {
        /* Doesn't exist; create one */
        new_size = 16;
    } else if (rv->header.count == rv->header.space &&
               rv->header.count < NSS_MAX_ERROR_STACK_COUNT) {
        /* Too small, expand it */
        new_size = PR_MIN(rv->header.space * 2, NSS_MAX_ERROR_STACK_COUNT);
    } else {
        /* Okay, return it */
        return rv;
    }

    new_bytes = (new_size * sizeof(PRInt32)) + sizeof(error_stack);
    /* Use NSPR's calloc/realloc, not NSS's, to avoid loops! */
    new_stack = PR_Calloc(1, new_bytes);

    if ((error_stack *)NULL != new_stack) {
        if ((error_stack *)NULL != rv) {
            (void)nsslibc_memcpy(new_stack, rv, rv->header.space);
        }
        new_stack->header.space = new_size;
    }

    /* Set the value, whether or not the allocation worked */
    PR_SetThreadPrivate(error_stack_index, new_stack);
    return new_stack;
}

/*
 * The error stack
 *
 * The public methods relating to the error stack are:
 *
 *  NSS_GetError
 *  NSS_GetErrorStack
 *
 * The nonpublic methods relating to the error stack are:
 *
 *  nss_SetError
 *  nss_ClearErrorStack
 *
 */

/*
 * NSS_GetError
 *
 * This routine returns the highest-level (most general) error set
 * by the most recent NSS library routine called by the same thread
 * calling this routine.
 *
 * This routine cannot fail.  However, it may return zero, which
 * indicates that the previous NSS library call did not set an error.
 *
 * Return value:
 *  0 if no error has been set
 *  A nonzero error number
 */

NSS_IMPLEMENT PRInt32
NSS_GetError(void)
{
    error_stack *es = error_get_my_stack();

    if ((error_stack *)NULL == es) {
        return NSS_ERROR_NO_MEMORY; /* Good guess! */
    }

    if (0 == es->header.count) {
        return 0;
    }

    return es->stack[es->header.count - 1];
}

/*
 * NSS_GetErrorStack
 *
 * This routine returns a pointer to an array of integers, containing
 * the entire sequence or "stack" of errors set by the most recent NSS
 * library routine called by the same thread calling this routine.
 * NOTE: the caller DOES NOT OWN the memory pointed to by the return
 * value.  The pointer will remain valid until the calling thread
 * calls another NSS routine.  The lowest-level (most specific) error
 * is first in the array, and the highest-level is last.  The array is
 * zero-terminated.  This routine may return NULL upon error; this
 * indicates a low-memory situation.
 *
 * Return value:
 *  NULL upon error, which is an implied NSS_ERROR_NO_MEMORY
 *  A NON-caller-owned pointer to an array of integers
 */

NSS_IMPLEMENT PRInt32 *
NSS_GetErrorStack(void)
{
    error_stack *es = error_get_my_stack();

    if ((error_stack *)NULL == es) {
        return (PRInt32 *)NULL;
    }

    /* Make sure it's terminated */
    es->stack[es->header.count] = 0;

    return es->stack;
}

/*
 * nss_SetError
 *
 * This routine places a new error code on the top of the calling
 * thread's error stack.  Calling this routine wiht an error code
 * of zero will clear the error stack.
 */

NSS_IMPLEMENT void
nss_SetError(PRUint32 error)
{
    error_stack *es;

    if (0 == error) {
        nss_ClearErrorStack();
        return;
    }

    es = error_get_my_stack();
    if ((error_stack *)NULL == es) {
        /* Oh, well. */
        return;
    }

    if (es->header.count < es->header.space) {
        es->stack[es->header.count++] = error;
    } else {
        memmove(es->stack, es->stack + 1,
                (es->header.space - 1) * (sizeof es->stack[0]));
        es->stack[es->header.space - 1] = error;
    }
    return;
}

/*
 * nss_ClearErrorStack
 *
 * This routine clears the calling thread's error stack.
 */

NSS_IMPLEMENT void
nss_ClearErrorStack(void)
{
    error_stack *es = error_get_my_stack();
    if ((error_stack *)NULL == es) {
        /* Oh, well. */
        return;
    }

    es->header.count = 0;
    es->stack[0] = 0;
    return;
}

/*
 * nss_DestroyErrorStack
 *
 * This routine frees the calling thread's error stack.
 */

NSS_IMPLEMENT void
nss_DestroyErrorStack(void)
{
    if (INVALID_TPD_INDEX != error_stack_index) {
        PR_SetThreadPrivate(error_stack_index, NULL);
        error_stack_index = INVALID_TPD_INDEX;
        error_call_once = error_call_again; /* allow to init again */
    }
    return;
}
