#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char output_string[1024];
void reset_output(void);
static void output(const char *fmt, ...);

// These are pointers to ContextHandles, which in turn have pointers to a
// Context as their first field
void** parent_regs;
void** child_regs;

void lucet_context_swap(void* from, void* to);
void lucet_context_set(void* to);

void arg_printing_child(void *arg0, void *arg1)
{
    int arg0_val = *(int *) arg0;
    int arg1_val = *(int *) arg1;

    output("hello from the child! my args were %d and %d\n", arg0_val, arg1_val);

    lucet_context_swap(*child_regs, *parent_regs);

    // Read the arguments again
    arg0_val = *(int *) arg0;
    arg1_val = *(int *) arg1;

    output("now they are %d and %d\n", arg0_val, arg1_val);

    lucet_context_swap(*child_regs, *parent_regs);
}

// Use the lucet_context_set function to jump to the parent without saving
// the child
void context_set_child()
{
    output("hello from the child! setting context to parent...\n");
    lucet_context_set(*parent_regs);
}

void returning_child()
{
    output("hello from the child! returning...\n");
}

void child_3_args(uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    output("the good three args boy %" PRId64 " %" PRId64 " %" PRId64 "\n", arg1, arg2, arg3);
}

void child_4_args(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    output("the large four args boy %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n", arg1, arg2,
           arg3, arg4);
}

void child_5_args(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    output("the big five args son %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n",
           arg1, arg2, arg3, arg4, arg5);
}

void child_6_args(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5,
                  uint64_t arg6)
{
    output("6 args, hahaha long boy %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64
           " %" PRId64 "\n",
           arg1, arg2, arg3, arg4, arg5, arg6);
}

void child_7_args(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5,
                  uint64_t arg6, uint64_t arg7)
{
    output("7 args, hahaha long boy %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64
           " %" PRId64 " %" PRId64 "\n",
           arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

void child_8_args(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5,
                  uint64_t arg6, uint64_t arg7, uint64_t arg8)
{
    output("8 args, hahaha long boy %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64
           " %" PRId64 " %" PRId64 " %" PRId64 "\n",
           arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}

void child_9_args(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5,
                  uint64_t arg6, uint64_t arg7, uint64_t arg8, uint64_t arg9)
{
    output("9 args, hahaha long boy %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64
           " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n",
           arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

void child_10_args(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5,
                   uint64_t arg6, uint64_t arg7, uint64_t arg8, uint64_t arg9, uint64_t arg10)
{
    output("10 args, hahaha very long boy %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64
           " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n",
           arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
}

// Helpers:
static char * output_cursor;
static size_t output_cursor_len;

void reset_output(void)
{
    memset(output_string, 0, sizeof(output_string));
    output_cursor     = output_string;
    output_cursor_len = sizeof(output_string);
}

static void output(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(output_cursor, output_cursor_len, fmt, args);
    if (res > 0) {
        output_cursor += res;
        output_cursor_len -= res;
    } else {
        abort();
    }
    va_end(args);
}
