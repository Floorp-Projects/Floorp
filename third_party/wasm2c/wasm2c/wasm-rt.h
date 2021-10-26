/*
 * Copyright 2018 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WASM_RT_H_
#define WASM_RT_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>

#if defined(_WIN32)
  #define WASM2C_FUNC_EXPORT __declspec(dllexport)
#else
  #define WASM2C_FUNC_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum stack depth before trapping. This can be configured by defining
 * this symbol before including wasm-rt when building the generated c files,
 * for example:
 *
 * ```
 *   cc -c -DWASM_RT_MAX_CALL_STACK_DEPTH=100 my_module.c -o my_module.o
 * ```
 * */
#ifndef WASM_RT_MAX_CALL_STACK_DEPTH
#define WASM_RT_MAX_CALL_STACK_DEPTH 500
#endif

/** Check if we should use guard page model.
 * This is enabled by default unless WASM_USE_EXPLICIT_BOUNDS_CHECKS is defined.
 */
#if defined(WASM_USE_GUARD_PAGES) && defined(WASM_USE_EXPLICIT_BOUNDS_CHECKS)
#  error "Cannot define both WASM_USE_GUARD_PAGES and WASM_USE_EXPLICIT_BOUNDS_CHECKS"
#elif !defined(WASM_USE_GUARD_PAGES) && !defined(WASM_USE_EXPLICIT_BOUNDS_CHECKS)
// default to guard pages
#  define WASM_USE_GUARD_PAGES
#endif

/** Define WASM_USE_INCREMENTAL_MOVEABLE_MEMORY_ALLOC if you want the runtime to incrementally allocate heap/linear memory
 * Note that this memory may be moved when it needs to expand
 */

#if defined(_MSC_VER)
#define WASM_RT_NO_RETURN __declspec(noreturn)
#else
#define WASM_RT_NO_RETURN __attribute__((noreturn))
#endif

/** Reason a trap occurred. Provide this to `wasm_rt_trap`.
 * If you update this enum also update the error message in wasm_rt_trap.
 */
typedef enum {
  WASM_RT_TRAP_NONE,                          /** No error. */
  WASM_RT_TRAP_OOB,                           /** Out-of-bounds access in linear memory. */
  WASM_RT_TRAP_INT_OVERFLOW,                  /** Integer overflow on divide or truncation. */
  WASM_RT_TRAP_DIV_BY_ZERO,                   /** Integer divide by zero. */
  WASM_RT_TRAP_INVALID_CONVERSION,            /** Conversion from NaN to integer. */
  WASM_RT_TRAP_UNREACHABLE,                   /** Unreachable instruction executed. */
  WASM_RT_TRAP_CALL_INDIRECT_TABLE_EXPANSION, /** Invalid call_indirect, as func table cannot grow/grow further. */
  WASM_RT_TRAP_CALL_INDIRECT_OOB_INDEX,       /** Invalid call_indirect, due to index larger than func table. */
  WASM_RT_TRAP_CALL_INDIRECT_NULL_PTR,        /** Invalid call_indirect, as function being invoked is null. */
  WASM_RT_TRAP_CALL_INDIRECT_TYPE_MISMATCH,   /** Invalid call_indirect, as function being invoked has an unexpected type. */
  WASM_RT_TRAP_CALL_INDIRECT_UNKNOWN_ERR,     /** Invalid call_indirect, for other reason. */
  WASM_RT_TRAP_EXHAUSTION,                    /** Call stack exhausted. */
  WASM_RT_TRAP_SHADOW_MEM,                    /** Trap due to shadow memory mismatch */
  WASM_RT_TRAP_WASI,                          /** Trap due to WASI error */
} wasm_rt_trap_t;

/** Value types. Used to define function signatures. */
typedef enum {
  WASM_RT_I32,
  WASM_RT_I64,
  WASM_RT_F32,
  WASM_RT_F64,
} wasm_rt_type_t;

/** A function type for all `anyfunc` functions in a Table. All functions are
 * stored in this canonical form, but must be cast to their proper signature to
 * call. */
typedef void (*wasm_rt_anyfunc_t)(void);

/**
 * The class of the indirect function being invoked
 */
typedef enum {
  WASM_RT_INTERNAL_FUNCTION,
  WASM_RT_EXTERNAL_FUNCTION
} wasm_rt_elem_target_class_t;

/** A single element of a Table. */
typedef struct {
  wasm_rt_elem_target_class_t func_class;
  /** The index as returned from `wasm_rt_register_func_type`. */
  uint32_t func_type;
  /** The function. The embedder must know the actual C signature of the
   * function and cast to it before calling. */
  wasm_rt_anyfunc_t func;
} wasm_rt_elem_t;

typedef uint8_t wasm2c_shadow_memory_cell_t;

typedef struct {
  wasm2c_shadow_memory_cell_t* data;
  size_t data_size;
  void* allocation_sizes_map;
  uint32_t heap_base;
} wasm2c_shadow_memory_t;

/** A Memory object. */
typedef struct {
  /** The linear memory data, with a byte length of `size`. */
#if defined(WASM_USE_GUARD_PAGES) || !defined(WASM_USE_INCREMENTAL_MOVEABLE_MEMORY_ALLOC)
  uint8_t* const data;
#else
  uint8_t* data;
#endif
  /** The current and maximum page count for this Memory object. If there is no
   * maximum, `max_pages` is 0xffffffffu (i.e. UINT32_MAX). */
  uint32_t pages, max_pages;
  /** The current size of the linear memory, in bytes. */
  uint32_t size;

#if defined(WASM_CHECK_SHADOW_MEMORY)
  wasm2c_shadow_memory_t shadow_memory;
#endif
} wasm_rt_memory_t;

/** A Table object. */
typedef struct {
  /** The table element data, with an element count of `size`. */
  wasm_rt_elem_t* data;
  /** The maximum element count of this Table object. If there is no maximum,
   * `max_size` is 0xffffffffu (i.e. UINT32_MAX). */
  uint32_t max_size;
  /** The current element count of the table. */
  uint32_t size;
} wasm_rt_table_t;

typedef struct wasm_func_type_t {
  wasm_rt_type_t* params;
  wasm_rt_type_t* results;
  uint32_t param_count;
  uint32_t result_count;
} wasm_func_type_t;

#define WASM2C_WASI_MAX_SETJMP_STACK 32
#define WASM2C_WASI_MAX_FDS 32
typedef struct wasm_sandbox_wasi_data {
  wasm_rt_memory_t* heap_memory;

  uint32_t tempRet0;

  uint32_t next_setjmp_index;
  jmp_buf setjmp_stack[WASM2C_WASI_MAX_SETJMP_STACK];

  uint32_t main_argc;
  char** main_argv;

  int wasm_fd_to_native[WASM2C_WASI_MAX_FDS];
  uint32_t next_wasm_fd;

  void* clock_data;

} wasm_sandbox_wasi_data;

typedef void (*wasm_rt_sys_init_t)(void);
typedef void* (*create_wasm2c_sandbox_t)(void);
typedef void (*destroy_wasm2c_sandbox_t)(void* sbx_ptr);
typedef void* (*lookup_wasm2c_nonfunc_export_t)(void* sbx_ptr, const char* name);
typedef uint32_t (*lookup_wasm2c_func_index_t)(void* sbx_ptr, uint32_t param_count, uint32_t result_count, wasm_rt_type_t* types);
typedef uint32_t (*add_wasm2c_callback_t)(void* sbx_ptr, uint32_t func_type_idx, void* func_ptr, wasm_rt_elem_target_class_t func_class);
typedef void (*remove_wasm2c_callback_t)(void* sbx_ptr, uint32_t callback_idx);

typedef struct wasm2c_sandbox_funcs_t {
  wasm_rt_sys_init_t wasm_rt_sys_init;
  create_wasm2c_sandbox_t create_wasm2c_sandbox;
  destroy_wasm2c_sandbox_t destroy_wasm2c_sandbox;
  lookup_wasm2c_nonfunc_export_t lookup_wasm2c_nonfunc_export;
  lookup_wasm2c_func_index_t lookup_wasm2c_func_index;
  add_wasm2c_callback_t add_wasm2c_callback;
  remove_wasm2c_callback_t remove_wasm2c_callback;
} wasm2c_sandbox_funcs_t;

/** Stop execution immediately and jump back to the call to `wasm_rt_try`.
 *  The result of `wasm_rt_try` will be the provided trap reason.
 *
 *  This is typically called by the generated code, and not the embedder. */
WASM_RT_NO_RETURN extern void wasm_rt_trap(wasm_rt_trap_t);

/** An indirect callback function failed.
 *  Deduce the reason for the failure and then call trap.
 *
 *  This is typically called by the generated code, and not the embedder. */
WASM_RT_NO_RETURN extern void wasm_rt_callback_error_trap(wasm_rt_table_t* table, uint32_t func_index, uint32_t expected_func_type);

/** Register a function type with the given signature. The returned function
 * index is guaranteed to be the same for all calls with the same signature.
 * The following varargs must all be of type `wasm_rt_type_t`, first the
 * params` and then the `results`.
 *
 *  ```
 *    // Register (func (param i32 f32) (result i64)).
 *    wasm_rt_register_func_type(2, 1, WASM_RT_I32, WASM_RT_F32, WASM_RT_I64);
 *    => returns 1
 *
 *    // Register (func (result i64)).
 *    wasm_rt_register_func_type(0, 1, WASM_RT_I32);
 *    => returns 2
 *
 *    // Register (func (param i32 f32) (result i64)) again.
 *    wasm_rt_register_func_type(2, 1, WASM_RT_I32, WASM_RT_F32, WASM_RT_I64);
 *    => returns 1
 *  ``` */
extern uint32_t wasm_rt_register_func_type(wasm_func_type_t** p_func_type_structs,
                                           uint32_t* p_func_type_count,
                                           uint32_t params,
                                           uint32_t results,
                                           wasm_rt_type_t* types);

void wasm_rt_cleanup_func_types(wasm_func_type_t** p_func_type_structs,
                               uint32_t* p_func_type_count);

/** Initialize a Memory object with an initial page size of `initial_pages` and
 * a maximum page size of `max_pages`.
 *
 *  ```
 *    wasm_rt_memory_t my_memory;
 *    // 1 initial page (65536 bytes), and a maximum of 2 pages.
 *    wasm_rt_allocate_memory(&my_memory, 1, 2);
 *  ``` */
extern bool wasm_rt_allocate_memory(wasm_rt_memory_t*,
                                    uint32_t initial_pages,
                                    uint32_t max_pages);

extern void wasm_rt_deallocate_memory(wasm_rt_memory_t*);

/** Grow a Memory object by `pages`, and return the previous page count. If
 * this new page count is greater than the maximum page count, the grow fails
 * and 0xffffffffu (UINT32_MAX) is returned instead.
 *
 *  ```
 *    wasm_rt_memory_t my_memory;
 *    ...
 *    // Grow memory by 10 pages.
 *    uint32_t old_page_size = wasm_rt_grow_memory(&my_memory, 10);
 *    if (old_page_size == UINT32_MAX) {
 *      // Failed to grow memory.
 *    }
 *  ``` */
extern uint32_t wasm_rt_grow_memory(wasm_rt_memory_t*, uint32_t pages);

/** Initialize a Table object with an element count of `elements` and a maximum
 * page size of `max_elements`.
 *
 *  ```
 *    wasm_rt_table_t my_table;
 *    // 5 elemnets and a maximum of 10 elements.
 *    wasm_rt_allocate_table(&my_table, 5, 10);
 *  ``` */
extern void wasm_rt_allocate_table(wasm_rt_table_t*,
                                   uint32_t elements,
                                   uint32_t max_elements);

extern void wasm_rt_deallocate_table(wasm_rt_table_t*);

extern void wasm_rt_expand_table(wasm_rt_table_t*);

// One time init function for wasm runtime. Should be called once for the current process
extern void wasm_rt_sys_init();

// Initialize wasi for the given sandbox. Called prior to sandbox execution.
extern void wasm_rt_init_wasi(wasm_sandbox_wasi_data*);

extern void wasm_rt_cleanup_wasi(wasm_sandbox_wasi_data*);

// Helper function that host can use to ensure wasm2c code is loaded correctly when using dynamic libraries
extern void wasm2c_ensure_linked();

// Runtime functions for shadow memory

// Create the shadow memory
extern void wasm2c_shadow_memory_create(wasm_rt_memory_t* mem);
// Expand the shadow memory to match wasm memory
extern void wasm2c_shadow_memory_expand(wasm_rt_memory_t* mem);
// Cleanup
extern void wasm2c_shadow_memory_destroy(wasm_rt_memory_t* mem);
// Perform checks for the load operation that completed
WASM2C_FUNC_EXPORT extern void wasm2c_shadow_memory_load(wasm_rt_memory_t* mem, const char* func_name, uint32_t ptr, uint32_t ptr_size);
// Perform checks for the store operation that completed
WASM2C_FUNC_EXPORT extern void wasm2c_shadow_memory_store(wasm_rt_memory_t* mem, const char* func_name, uint32_t ptr, uint32_t ptr_size);
// Mark an area as allocated, if it is currently unused. If already used, this is a noop.
extern void wasm2c_shadow_memory_reserve(wasm_rt_memory_t* mem, uint32_t ptr, uint32_t ptr_size);
// Perform checks for the malloc operation that completed
extern void wasm2c_shadow_memory_dlmalloc(wasm_rt_memory_t* mem, uint32_t ptr, uint32_t ptr_size);
// Perform checks for the free operation that will be run
extern void wasm2c_shadow_memory_dlfree(wasm_rt_memory_t* mem, uint32_t ptr);
// Pass on information about the boundary between wasm globals and heap
// Shadow asan will check that all malloc metadata structures below this boundary are only accessed by malloc related functions
extern void wasm2c_shadow_memory_mark_globals_heap_boundary(wasm_rt_memory_t* mem, uint32_t ptr);
// Print a list of all allocations currently active
WASM2C_FUNC_EXPORT extern void wasm2c_shadow_memory_print_allocations(wasm_rt_memory_t* mem);

#ifdef __cplusplus
}
#endif

#endif /* WASM_RT_H_ */
