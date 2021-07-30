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

#include "wasm-rt.h"
#include "wasm-rt-os.h"

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 65536

void wasm_rt_trap(wasm_rt_trap_t code) {
  assert(code != WASM_RT_TRAP_NONE);
  abort();
}

static bool func_types_are_equal(wasm_func_type_t* a, wasm_func_type_t* b) {
  if (a->param_count != b->param_count || a->result_count != b->result_count)
    return 0;
  uint32_t i;
  for (i = 0; i < a->param_count; ++i)
    if (a->params[i] != b->params[i])
      return 0;
  for (i = 0; i < a->result_count; ++i)
    if (a->results[i] != b->results[i])
      return 0;
  return 1;
}

uint32_t wasm_rt_register_func_type(wasm_func_type_t** p_func_type_structs,
                                    uint32_t* p_func_type_count,
                                    uint32_t param_count,
                                    uint32_t result_count,
                                    wasm_rt_type_t* types) {
  wasm_func_type_t func_type;
  func_type.param_count = param_count;
  func_type.params = malloc(param_count * sizeof(wasm_rt_type_t));
  func_type.result_count = result_count;
  func_type.results = malloc(result_count * sizeof(wasm_rt_type_t));

  uint32_t i;
  for (i = 0; i < param_count; ++i)
    func_type.params[i] = types[i];
  for (i = 0; i < result_count; ++i)
    func_type.results[i] = types[(uint64_t)(param_count) + i];

  for (i = 0; i < *p_func_type_count; ++i) {
    wasm_func_type_t* func_types = *p_func_type_structs;
    if (func_types_are_equal(&func_types[i], &func_type)) {
      free(func_type.params);
      free(func_type.results);
      return i + 1;
    }
  }

  uint32_t idx = (*p_func_type_count)++;
  // realloc works fine even if *p_func_type_structs is null
  *p_func_type_structs = realloc(*p_func_type_structs, *p_func_type_count * sizeof(wasm_func_type_t));
  (*p_func_type_structs)[idx] = func_type;
  return idx + 1;
}

void wasm_rt_cleanup_func_types(wasm_func_type_t** p_func_type_structs, uint32_t* p_func_type_count) {
  // Use a u64 to iterate over u32 arrays to prevent infinite loops
  const uint32_t func_count = *p_func_type_count;
  for (uint64_t idx = 0; idx < func_count; idx++){
    wasm_func_type_t* func_type = &((*p_func_type_structs)[idx]);
    if (func_type->params != 0) {
      free(func_type->params);
      func_type->params = 0;
    }
    if (func_type->results != 0) {
      free(func_type->results);
      func_type->results = 0;
    }
  }
  free(*p_func_type_structs);
}

void wasm_rt_allocate_memory(wasm_rt_memory_t* memory,
                             uint32_t initial_pages,
                             uint32_t max_pages) {
  uint32_t byte_length = initial_pages * PAGE_SIZE;
#if WASM_USING_GUARD_PAGES == 1
  /* Reserve 8GiB. */
  const uint64_t heap_alignment = 0x100000000ul;
  void* addr = NULL;
  const uint64_t retries = 10;

  for (uint64_t i = 0; i < retries; i++) {
    addr = os_mmap_aligned(NULL, 0x200000000ul, MMAP_PROT_NONE, MMAP_MAP_NONE, heap_alignment, 0 /* alignment_offset */);
    if (addr) {
      break;
    }
  }

  if (!addr) {
    perror("mmap failed");
    abort();
  }
  int ret = os_mmap_commit(addr, byte_length, MMAP_PROT_READ | MMAP_PROT_WRITE);
  if (ret != 0) {
    abort();
  }
  // This is a valid way to initialize a constant field that is not undefined behavior
  // https://stackoverflow.com/questions/9691404/how-to-initialize-const-in-a-struct-in-c-with-malloc
  // Summary: malloc of a struct, followed by a write to the constant fields is still defined behavior iff
  //   there is no prior read of the field
  *(uint8_t**) &memory->data = addr;
#else
  memory->data = calloc(byte_length, 1);
#endif
  memory->size = byte_length;
  memory->pages = initial_pages;
  memory->max_pages = max_pages;

#if defined(WASM_CHECK_SHADOW_MEMORY)
  wasm2c_shadow_memory_create(memory);
#endif
}

void wasm_rt_deallocate_memory(wasm_rt_memory_t* memory) {
#if WASM_USING_GUARD_PAGES == 1
  os_munmap(memory->data, 0x200000000ul);
#else
  os_munmap(memory->data, memory->size);
#endif
#if defined(WASM_CHECK_SHADOW_MEMORY)
  wasm2c_shadow_memory_destroy(memory);
#endif
}

uint32_t wasm_rt_grow_memory(wasm_rt_memory_t* memory, uint32_t delta) {
  uint32_t old_pages = memory->pages;
  uint32_t new_pages = memory->pages + delta;
  if (new_pages == 0) {
    return 0;
  }
  if (new_pages < old_pages || new_pages > memory->max_pages) {
    return (uint32_t)-1;
  }
  uint32_t old_size = old_pages * PAGE_SIZE;
  uint32_t new_size = new_pages * PAGE_SIZE;
  uint32_t delta_size = delta * PAGE_SIZE;

#if WASM_USING_GUARD_PAGES == 1
  int ret = os_mmap_commit(memory->data + old_size, delta_size, MMAP_PROT_READ | MMAP_PROT_WRITE);
  if (ret != 0) {
    return (uint32_t)-1;
  }
#else
  uint8_t* new_data = realloc(memory->data, new_size);
  if (new_data == NULL) {
    return (uint32_t)-1;
  }
  #if !WABT_BIG_ENDIAN
    memset(new_data + old_size, 0, delta_size);
  #endif
  memory->data = new_data;
#endif

#if WABT_BIG_ENDIAN
  memmove(memory->data + new_size - old_size, memory->data, old_size);
  memset(memory->data, 0, delta_size);
#endif
  memory->pages = new_pages;
  memory->size = new_size;
#if defined(WASM_CHECK_SHADOW_MEMORY)
  wasm2c_shadow_memory_expand(memory);
#endif
  return old_pages;
}

void wasm_rt_allocate_table(wasm_rt_table_t* table,
                            uint32_t elements,
                            uint32_t max_elements) {
  assert(max_elements >= elements);
  table->size = elements;
  table->max_size = max_elements;
  table->data = calloc(table->size, sizeof(wasm_rt_elem_t));
  assert(table->data != 0);
}

void wasm_rt_deallocate_table(wasm_rt_table_t* table) {
  free(table->data);
}

#define SATURATING_U32_ADD(ret_ptr, a, b) { \
  if ((a) > (UINT32_MAX - (b))) {             \
    /* add will overflowed */               \
    *ret_ptr = UINT32_MAX;                  \
  } else {                                  \
    *ret_ptr = (a) + (b);                   \
  }                                         \
}

#define CHECKED_U32_RET_SIZE_T_MULTIPLY(ret_ptr, a, b) { \
  if ((a) > (SIZE_MAX / (b))) {                            \
    /* multiple will overflowed */                       \
    wasm_rt_trap(WASM_RT_TRAP_CALL_INDIRECT);            \
  } else {                                               \
    /* convert to size by assigning */                   \
    *ret_ptr = a;                                        \
    *ret_ptr = *ret_ptr * b;                             \
  }                                                      \
}

void wasm_rt_expand_table(wasm_rt_table_t* table) {
  uint32_t new_size = 0;
  SATURATING_U32_ADD(&new_size, table->size, 32);

  if (new_size > table->max_size) {
    new_size = table->max_size;
  }

  if (table->size == new_size) {
    // table is already as large as we allowed, can't expand further
    wasm_rt_trap(WASM_RT_TRAP_CALL_INDIRECT);
  }

  size_t allocation_size = 0;
  CHECKED_U32_RET_SIZE_T_MULTIPLY(&allocation_size, sizeof(wasm_rt_elem_t), new_size);
  table->data = realloc(table->data, allocation_size);
  assert(table->data != 0);

  memset(&(table->data[table->size]), 0, allocation_size - (table->size * sizeof(wasm_rt_elem_t)));
  table->size = new_size;
}

void wasm2c_ensure_linked() {
  // We use this to ensure the dynamic library with the wasi symbols is loaded for the host application
}

#undef PAGE_SIZE
