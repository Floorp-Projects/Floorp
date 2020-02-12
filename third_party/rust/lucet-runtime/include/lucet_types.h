#ifndef LUCET_TYPES_H
#define LUCET_TYPES_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

enum lucet_error {
    lucet_error_ok,
    lucet_error_invalid_argument,
    lucet_error_region_full,
    lucet_error_module,
    lucet_error_limits_exceeded,
    lucet_error_no_linear_memory,
    lucet_error_symbol_not_found,
    lucet_error_func_not_found,
    lucet_error_runtime_fault,
    lucet_error_runtime_terminated,
    lucet_error_dl,
    lucet_error_instance_not_returned,
    lucet_error_instance_not_yielded,
    lucet_error_start_yielded,
    lucet_error_internal,
    lucet_error_unsupported,
};

enum lucet_signal_behavior {
    lucet_signal_behavior_default,
    lucet_signal_behavior_continue,
    lucet_signal_behavior_terminate,
};

enum lucet_terminated_reason {
    lucet_terminated_reason_signal,
    lucet_terminated_reason_ctx_not_found,
    lucet_terminated_reason_yield_type_mismatch,
    lucet_terminated_reason_borrow_error,
    lucet_terminated_reason_provided,
};

enum lucet_trapcode {
    lucet_trapcode_stack_overflow,
    lucet_trapcode_heap_out_of_bounds,
    lucet_trapcode_out_of_bounds,
    lucet_trapcode_indirect_call_to_null,
    lucet_trapcode_bad_signature,
    lucet_trapcode_integer_overflow,
    lucet_trapcode_integer_div_by_zero,
    lucet_trapcode_bad_conversion_to_integer,
    lucet_trapcode_interrupt,
    lucet_trapcode_table_out_of_bounds,
    lucet_trapcode_user,
    lucet_trapcode_unknown,
};

enum lucet_val_type {
    lucet_val_type_c_ptr,
    lucet_val_type_guest_ptr,
    lucet_val_type_u8,
    lucet_val_type_u16,
    lucet_val_type_u32,
    lucet_val_type_u64,
    lucet_val_type_i8,
    lucet_val_type_i16,
    lucet_val_type_i32,
    lucet_val_type_i64,
    lucet_val_type_usize,
    lucet_val_type_isize,
    lucet_val_type_bool,
    lucet_val_type_f32,
    lucet_val_type_f64,
};

union lucet_val_inner_val {
    void *   as_c_ptr;
    uint64_t as_u64;
    int64_t  as_i64;
    float    as_f32;
    double   as_f64;
};

struct lucet_val {
    enum lucet_val_type       ty;
    union lucet_val_inner_val inner_val;
};

struct lucet_dl_module;

struct lucet_instance;

struct lucet_region;

/**
 * Runtime limits for the various memories that back a Lucet instance.
 * Each value is specified in bytes, and must be evenly divisible by the host page size (4K).
 */
struct lucet_alloc_limits {
    /**
     * Max size of the heap, which can be backed by real memory. (default 1M)
     */
    uint64_t heap_memory_size;
    /**
     * Size of total virtual memory. (default 8G)
     */
    uint64_t heap_address_space_size;
    /**
     * Size of the guest stack. (default 128K)
     */
    uint64_t stack_size;
    /**
     * Size of the globals region in bytes; each global uses 8 bytes. (default 4K)
     */
    uint64_t globals_size;
    /**
     * Size of the signal stack region in bytes.
     *
     * SIGSTKSZ from <signals.h> is a good default when linking against a Rust release build of
     * lucet-runtime, but 12K or more is recommended when using a Rust debug build.
     */
    uint64_t signal_stack_size;
};

typedef enum lucet_signal_behavior (*lucet_signal_handler)(struct lucet_instance *   inst,
                                                           const enum lucet_trapcode trap,
                                                           int signum, const siginfo_t *siginfo,
                                                           const void *context);

typedef void (*lucet_fatal_handler)(struct lucet_instance *inst);

struct lucet_untyped_retval {
    char fp[16];
    char gp[8];
};

#define LUCET_MODULE_ADDR_DETAILS_NAME_LEN 256

struct lucet_module_addr_details {
    bool module_code_resolvable;
    bool in_module_code;
    char file_name[LUCET_MODULE_ADDR_DETAILS_NAME_LEN];
    char sym_name[LUCET_MODULE_ADDR_DETAILS_NAME_LEN];
};

struct lucet_runtime_faulted {
    bool                             fatal;
    enum lucet_trapcode              trapcode;
    uintptr_t                        rip_addr;
    struct lucet_module_addr_details rip_addr_details;
};

struct lucet_terminated {
    enum lucet_terminated_reason reason;
    void *                       provided;
};

struct lucet_yielded {
    void *val;
};

union lucet_result_val {
    struct lucet_untyped_retval  returned;
    struct lucet_yielded         yielded;
    struct lucet_runtime_faulted faulted;
    struct lucet_terminated      terminated;
    enum lucet_error             errored;
};

enum lucet_result_tag {
    lucet_result_tag_returned,
    lucet_result_tag_yielded,
    lucet_result_tag_faulted,
    lucet_result_tag_terminated,
    lucet_result_tag_errored,
};

struct lucet_result {
    enum lucet_result_tag  tag;
    union lucet_result_val val;
};

union lucet_retval_gp {
    char     as_untyped[8];
    void *   as_c_ptr;
    uint64_t as_u64;
    int64_t  as_i64;
};

#endif /* LUCET_TYPES_H */
