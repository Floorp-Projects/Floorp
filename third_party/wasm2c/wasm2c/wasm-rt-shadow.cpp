// Assume that this file is called only when shadow memory is used
#ifndef WASM_CHECK_SHADOW_MEMORY
#define WASM_CHECK_SHADOW_MEMORY
#endif

#include "wasm-rt.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <functional>

typedef enum class ALLOC_STATE : uint8_t {
  UNINIT = 0, ALLOCED, INITIALIZED,
} ALLOC_STATE;

static const char * alloc_state_strings[] = {
  "UNINIT", "ALLOCED", "INITIALIZED",
};

typedef enum class USED_STATE : uint8_t {
  UNUSED = 0, FREED
} USED_STATE;

static const char * used_state_strings[] = {
  "UNUSED", "FREED"
};

typedef enum class OWN_STATE : uint8_t {
  UNKNOWN = 0, PROGRAM, MALLOC
} OWN_STATE;

static const char * own_state_strings[] = {
  "UNKNOWN", "PROGRAM", "MALLOC"
};

typedef struct {
  ALLOC_STATE alloc_state;
  USED_STATE used_state;
  OWN_STATE own_state;
} cell_data_t;

static wasm2c_shadow_memory_cell_t pack(cell_data_t data) {
  uint8_t alloc_bits = ((uint8_t)data.alloc_state) & 0b11;
  uint8_t used_bits = (((uint8_t)data.used_state) & 0b11) << 2;
  uint8_t own_bits = (((uint8_t)data.own_state) & 0b11) << 4;
  uint8_t ret = alloc_bits | used_bits | own_bits;
  return ret;
}

static cell_data_t unpack(wasm2c_shadow_memory_cell_t byte) {
  uint8_t alloc_bits = byte & 0b11;
  uint8_t used_bits = (byte & 0b1100) >> 2;
  uint8_t own_bits = (byte & 0b110000) >> 4;
  cell_data_t ret { (ALLOC_STATE) alloc_bits, (USED_STATE) used_bits, (OWN_STATE) own_bits};
  return ret;
}

void wasm2c_shadow_memory_create(wasm_rt_memory_t* mem) {
  uint64_t new_size = ((uint64_t) mem->size) * sizeof(wasm2c_shadow_memory_cell_t);
  mem->shadow_memory.data = (wasm2c_shadow_memory_cell_t*) calloc(new_size, 1);
  assert(mem->shadow_memory.data != 0);
  mem->shadow_memory.data_size = new_size;
  mem->shadow_memory.allocation_sizes_map = new std::map<uint32_t, uint32_t>;
  mem->shadow_memory.heap_base = 0;
}

void wasm2c_shadow_memory_expand(wasm_rt_memory_t* mem) {
  uint64_t new_size = ((uint64_t) mem->size) * sizeof(wasm2c_shadow_memory_cell_t);
  uint8_t* new_data = (uint8_t*) realloc(mem->shadow_memory.data, new_size);
  assert(mem->shadow_memory.data != 0);
  uint64_t old_size = mem->shadow_memory.data_size;
  memset(new_data + old_size, 0, new_size - old_size);
  mem->shadow_memory.data = new_data;
  mem->shadow_memory.data_size = new_size;
}

#define allocation_sizes_map(mem) ((std::map<uint32_t, uint32_t>*) mem->shadow_memory.allocation_sizes_map)

void wasm2c_shadow_memory_destroy(wasm_rt_memory_t* mem) {
  free(mem->shadow_memory.data);
  mem->shadow_memory.data = 0;
  mem->shadow_memory.data_size = 0;
  delete allocation_sizes_map(mem);
  mem->shadow_memory.allocation_sizes_map = 0;
  mem->shadow_memory.heap_base = 0;
}

typedef void (iterate_t)(uint32_t index, cell_data_t* data);

static inline void memory_state_iterate(wasm_rt_memory_t* mem, uint32_t ptr, uint32_t ptr_size, std::function<iterate_t> callback) {
  uint64_t max = ((uint64_t) ptr) + ptr_size;
  assert(max <= UINT32_MAX);

  assert(max <= mem->shadow_memory.data_size);

  for (uint32_t i = ptr; i < ptr + ptr_size; i++) {
    wasm2c_shadow_memory_cell_t curr_state = mem->shadow_memory.data[i];
    cell_data_t unpacked = unpack(curr_state);
    callback(i, &unpacked);
    wasm2c_shadow_memory_cell_t new_state = pack(unpacked);
    mem->shadow_memory.data[i] = new_state;
  }
}

void wasm2c_shadow_memory_reserve(wasm_rt_memory_t* mem, uint32_t ptr, uint32_t ptr_size) {
  memory_state_iterate(mem, ptr, ptr_size, [&](uint32_t index, cell_data_t* data){
    if (data->alloc_state == ALLOC_STATE::UNINIT) {
      data->alloc_state = ALLOC_STATE::ALLOCED;
    }
  });
}

static void report_error(wasm_rt_memory_t* mem, const char* func_name, const char* error_message, uint32_t index, cell_data_t* data) {
  const char* alloc_state_string = "<>";
  const char* used_state_string = "<>";
  const char* own_state_string = "<>";

  if (data != 0) {
    alloc_state_string = alloc_state_strings[(int) data->alloc_state];
    used_state_string = used_state_strings[(int) data->used_state];
    own_state_string = own_state_strings[(int) data->own_state];
  }
  const uint64_t used_mem = wasm2c_shadow_memory_print_total_allocations(mem);
  printf("WASM Shadow memory ASAN failed! %s (Func: %s, Index: %" PRIu32 ") (Cell state: %s, %s, %s) (Allocated mem: %" PRIu64 ")!\n",
    error_message, func_name, index,
    alloc_state_string, used_state_string, own_state_string,
    used_mem
  );
  fflush(stdout);
  #ifndef WASM_CHECK_SHADOW_MEMORY_NO_ABORT_ON_FAIL
    wasm_rt_trap(WASM_RT_TRAP_SHADOW_MEM);
  #endif
}

void wasm2c_shadow_memory_dlmalloc(wasm_rt_memory_t* mem, uint32_t ptr, uint32_t ptr_size) {
  if (ptr == 0) {
    // malloc failed
    return;
  }

  const char* func_name = "<MALLOC>";
  if (ptr < mem->shadow_memory.heap_base) {
    report_error(mem, func_name, "malloc returning a pointer outside the heap", ptr, 0);
  }

  memory_state_iterate(mem, ptr, ptr_size, [&](uint32_t index, cell_data_t* data){
    if (data->alloc_state != ALLOC_STATE::UNINIT) {
      report_error(mem, func_name, "Malloc returned a pointer in already occupied memory!", index, data);
    } else {
      data->alloc_state = ALLOC_STATE::ALLOCED;
    }
  });

  auto alloc_map = allocation_sizes_map(mem);
  (*alloc_map)[ptr] = ptr_size;
}

void wasm2c_shadow_memory_dlfree(wasm_rt_memory_t* mem, uint32_t ptr) {
  if (ptr == 0) {
    // free noop
    return;
  }

  const char* func_name = "<FREE>";

  auto alloc_map = allocation_sizes_map(mem);
  auto it = alloc_map->find(ptr);
  if (it == alloc_map->end()) {
    report_error(mem, func_name, "Freeing a pointer that was not allocated!", ptr, 0);
  } else {
    uint32_t ptr_size = it->second;
    alloc_map->erase(it);

    memory_state_iterate(mem, ptr, ptr_size, [&](uint32_t index, cell_data_t* data){
      if (data->alloc_state == ALLOC_STATE::UNINIT) {
        report_error(mem, func_name, "Freeing uninitialized memory", index, data);
      } else {
        data->alloc_state = ALLOC_STATE::UNINIT;
      }

      // This memory is now "previously used" memory
      data->used_state = USED_STATE::FREED;
    });
  }
}

void wasm2c_shadow_memory_mark_globals_heap_boundary(wasm_rt_memory_t* mem, uint32_t ptr) {
  mem->shadow_memory.heap_base = ptr;
  wasm2c_shadow_memory_reserve(mem, 0, ptr);
}

static void check_heap_base_straddle(wasm_rt_memory_t* mem, const char* func_name, uint32_t ptr, uint32_t ptr_size) {
  uint32_t heap_base = mem->shadow_memory.heap_base;
  uint64_t max = ((uint64_t) ptr) + ptr_size;
  assert(max <= UINT32_MAX);

  if (ptr < heap_base) {
    if (ptr + ptr_size > heap_base) {
      report_error(mem, func_name, "Memory operation straddling heap base!", ptr, 0 /* cell_data */);
    }
  }
}

static bool is_malloc_family_function(const char* func_name, bool include_calloc, bool include_realloc) {
  if(strcmp(func_name, "w2c_dlmalloc") == 0 || strcmp(func_name, "w2c_dlfree") == 0 || strcmp(func_name, "w2c_sbrk") == 0) {
    return true;
  }
  if (include_calloc) {
    if(strcmp(func_name, "w2c_calloc") == 0) {
      return true;
    }
  }
  if (include_realloc) {
    if(strcmp(func_name, "w2c_realloc") == 0) {
      return true;
    }
  }
  return false;
}

// Functions that intentionally read beyond the end of an allocation for performance
// The limit is upto 7 bytes past the end of the allocation
static bool is_overread_function(const char* func_name) {
  return strcmp(func_name, "w2c_strlen") == 0;
}

WASM2C_FUNC_EXPORT void wasm2c_shadow_memory_load(wasm_rt_memory_t* mem, const char* func_name, uint32_t ptr, uint32_t ptr_size) {
  check_heap_base_straddle(mem, func_name, ptr, ptr_size);

  bool malloc_read_family = is_malloc_family_function(func_name, true, true);
  bool malloc_core_family = is_malloc_family_function(func_name, false, false);
  bool malloc_any_family  = malloc_read_family;
  bool overread_func_family = is_overread_function(func_name);

  memory_state_iterate(mem, ptr, ptr_size, [&](uint32_t index, cell_data_t* data){
    // Is this function exempt from checking
    bool exempt = false;

    // Malloc family of functions store chunk related information in the heap
    // These look like unitialized operations
    if (index >= mem->shadow_memory.heap_base && malloc_read_family) {
      exempt = true;
    }

    bool is_first_iteration = index == ptr;
    // Functions that intentionally read beyond the end of an allocation for performance
    // The limit is upto 7 bytes past the end of the allocation
    if (overread_func_family && !is_first_iteration) {
      exempt = true;
    }

    if (!exempt) {
#ifdef WASM_CHECK_SHADOW_MEMORY_UNINIT_READ
      if (data->alloc_state != ALLOC_STATE::INITIALIZED) {
        report_error(mem, func_name, "Reading uninitialized or unallocated memory", index, data);
      }
#else
      if (data->alloc_state == ALLOC_STATE::UNINIT) {
        report_error(mem, func_name, "Reading uninitialized memory", index, data);
      }
#endif
    }

    // Accessing C globals --- infer if this is malloc or program data
    if (index < mem->shadow_memory.heap_base) {
      if (malloc_core_family) {
        // Accessing C globals from core malloc functions
        if(data->own_state == OWN_STATE::PROGRAM) {
          report_error(mem, func_name, "Core malloc functions reading non malloc globals", index, data);
        }
        data->own_state = OWN_STATE::MALLOC;
      } else if (malloc_any_family) {
        // Accessing C globals from malloc wrapper functions
        // hard to infer as these access both regular and chunk memory, so leave things unchanged
      } else {
        if(data->own_state == OWN_STATE::MALLOC) {
          report_error(mem, func_name, "Program reading malloc globals", index, data);
        }
        data->own_state = OWN_STATE::PROGRAM;
      }
    }
  });
}

WASM2C_FUNC_EXPORT void wasm2c_shadow_memory_store(wasm_rt_memory_t* mem, const char* func_name, uint32_t ptr, uint32_t ptr_size) {
  check_heap_base_straddle(mem, func_name, ptr, ptr_size);

  bool malloc_write_family = is_malloc_family_function(func_name, false /* calloc shouldn't modify metadata */, true);
  bool malloc_core_family = is_malloc_family_function(func_name, false, false);
  bool malloc_any_family  = is_malloc_family_function(func_name, true, true);

  memory_state_iterate(mem, ptr, ptr_size, [&](uint32_t index, cell_data_t* data){
    // Is this function exempt from checking
    bool exempt = false;

    // Malloc family of functions store chunk related information in the heap
    // These look like unitialized operations
    if (index >= mem->shadow_memory.heap_base && malloc_write_family) {
      exempt = true;
    }

    if (!exempt) {
      if (data->alloc_state == ALLOC_STATE::UNINIT) {
        report_error(mem, func_name, "Writing uninitialized memory", index, data);
      } else if (data->alloc_state == ALLOC_STATE::ALLOCED) {
        data->alloc_state = ALLOC_STATE::INITIALIZED;
      }
    }

    // Accessing C globals --- infer if this is malloc or program data
    if (index < mem->shadow_memory.heap_base) {
      if (malloc_core_family) {
        // Accessing C globals from core malloc functions
        if(data->own_state == OWN_STATE::PROGRAM) {
          report_error(mem, func_name, "Core malloc functions writing non malloc globals", index, data);
        }
        data->own_state = OWN_STATE::MALLOC;
      } else if (malloc_any_family) {
        // Accessing C globals from malloc wrapper functions
        // hard to infer as these access both regular and chunk memory, so leave things unchanged
      } else {
        if(data->own_state == OWN_STATE::MALLOC) {
          report_error(mem, func_name, "Program writing malloc globals", index, data);
        }
        data->own_state = OWN_STATE::PROGRAM;
      }
    }
  });
}

WASM2C_FUNC_EXPORT void wasm2c_shadow_memory_print_allocations(wasm_rt_memory_t* mem) {
  auto alloc_map = allocation_sizes_map(mem);
  puts("{ ");
  int counter = 0;
  for (auto i = alloc_map->begin(); i != alloc_map->end(); ++i)
  {
    printf("%" PRIu32 ": %" PRIu32 ", ", i->first, i->second);
    counter++;
    if (counter >= 40) {
      counter = 0;
      puts("\n");
    }
  }
  puts(" }");
}

WASM2C_FUNC_EXPORT uint64_t wasm2c_shadow_memory_print_total_allocations(wasm_rt_memory_t* mem) {
  auto alloc_map = allocation_sizes_map(mem);
  uint64_t used_memory = mem->shadow_memory.heap_base;
  for (auto i = alloc_map->begin(); i != alloc_map->end(); ++i)
  {
    used_memory += i->second;
  }
  return used_memory;
}
