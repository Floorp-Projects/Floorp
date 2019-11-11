#ifndef LUCET_VMCTX_H
#define LUCET_VMCTX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Typedefs for use in hostcall signatures (host functions called from the
// guest). Instances always have a 32-bit pointer size into their heap, and
// 32-bit size_t type.
typedef uint32_t guest_int;
typedef uint32_t guest_ptr_t;
typedef uint32_t guest_size_t;

struct lucet_vmctx;

// Get a pointer to the instance heap.
char *lucet_vmctx_get_heap(struct lucet_vmctx const *);

// Check if a memory region is inside the instance heap.
bool lucet_vmctx_check_heap(struct lucet_vmctx const *, void *ptr, size_t len);

// Get the embedding context for a given instance
// TODO: rename
void *lucet_vmctx_get_delegate(struct lucet_vmctx const *);

void lucet_vmctx_terminate(struct lucet_vmctx const *, void *info);

void *lucet_vmctx_yield(struct lucet_vmctx const *, void *val);

// returns the current number of wasm pages
uint32_t lucet_vmctx_current_memory(struct lucet_vmctx const *);

// takes the number of wasm pages to grow by. returns the number of pages before
// the call on success, or -1 on failure.
int32_t lucet_vmctx_grow_memory(struct lucet_vmctx const *, uint32_t additional_pages);

// returns the address of a function given its ID
void *lucet_vmctx_get_func_from_idx(struct lucet_vmctx const *ctx, uint32_t table_id,
                                    uint32_t func_id);

// Mostly for tests - this conversion is builtin to lucetc
int64_t *lucet_vmctx_get_globals(struct lucet_vmctx const *ctx);

#endif // LUCET_VMCTX_H
