// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jpegli/memory_manager.h"

#include <string.h>

#include <vector>

#include "lib/jpegli/error.h"

struct jvirt_sarray_control {
  JSAMPARRAY full_buffer;
  size_t numrows;
  JDIMENSION maxaccess;
};

struct jvirt_barray_control {
  JBLOCKARRAY full_buffer;
  size_t numrows;
  JDIMENSION maxaccess;
};

namespace jpegli {

namespace {

struct MemoryManager {
  struct jpeg_memory_mgr pub;
  std::vector<std::vector<void*>> owned_ptrs;
};

void CheckPoolId(j_common_ptr cinfo, int pool_id) {
  if (pool_id < 0 || pool_id >= JPOOL_NUMPOOLS) {
    JPEGLI_ERROR("Invalid pool id %d", pool_id);
  }
}

void* Alloc(j_common_ptr cinfo, int pool_id, size_t sizeofobject) {
  MemoryManager* mem = reinterpret_cast<MemoryManager*>(cinfo->mem);
  CheckPoolId(cinfo, pool_id);
  void* p = malloc(sizeofobject);
  if (p == nullptr) {
    JPEGLI_ERROR("Out of memory");
  }
  mem->owned_ptrs[pool_id].push_back(p);
  return p;
}

template <typename T>
T** Alloc2dArray(j_common_ptr cinfo, int pool_id, JDIMENSION samplesperrow,
                 JDIMENSION numrows) {
  T** array = Allocate<T*>(cinfo, numrows, pool_id);
  for (size_t i = 0; i < numrows; ++i) {
    array[i] = Allocate<T>(cinfo, samplesperrow, pool_id);
  }
  return array;
}

template <typename Control, typename T>
Control* RequestVirtualArray(j_common_ptr cinfo, int pool_id, boolean pre_zero,
                             JDIMENSION samplesperrow, JDIMENSION numrows,
                             JDIMENSION maxaccess) {
  if (pool_id != JPOOL_IMAGE) {
    JPEGLI_ERROR("Only image lifetime virtual arrays are supported.");
  }
  Control* p = Allocate<Control>(cinfo, 1, pool_id);
  p->full_buffer = Alloc2dArray<T>(cinfo, pool_id, samplesperrow, numrows);
  p->numrows = numrows;
  p->maxaccess = maxaccess;
  if (pre_zero) {
    for (size_t i = 0; i < numrows; ++i) {
      memset(p->full_buffer[i], 0, samplesperrow * sizeof(T));
    }
  }
  return p;
}

void RealizeVirtualArrays(j_common_ptr cinfo) {
  // Nothing to do, the full arrays were realized at request time already.
}

template <typename Control, typename T>
T** AccessVirtualArray(j_common_ptr cinfo, Control* ptr, JDIMENSION start_row,
                       JDIMENSION num_rows, boolean writable) {
  if (num_rows > ptr->maxaccess) {
    JPEGLI_ERROR("Invalid virtual array access, num rows %u vs max rows %u",
                 num_rows, ptr->maxaccess);
  }
  if (start_row + num_rows > ptr->numrows) {
    JPEGLI_ERROR("Invalid virtual array access, %u vs %u total rows",
                 start_row + num_rows, ptr->numrows);
  }
  if (ptr->full_buffer == nullptr) {
    JPEGLI_ERROR("Invalid virtual array access, array not realized.");
  }
  return ptr->full_buffer + start_row;
}

void FreePool(j_common_ptr cinfo, int pool_id) {
  MemoryManager* mem = reinterpret_cast<MemoryManager*>(cinfo->mem);
  CheckPoolId(cinfo, pool_id);
  for (void* ptr : mem->owned_ptrs[pool_id]) {
    free(ptr);
  }
  mem->owned_ptrs[pool_id].clear();
}

void SelfDestruct(j_common_ptr cinfo) {
  MemoryManager* mem = reinterpret_cast<MemoryManager*>(cinfo->mem);
  for (int pool_id = 0; pool_id < JPOOL_NUMPOOLS; ++pool_id) {
    FreePool(cinfo, pool_id);
  }
  delete mem;
  cinfo->mem = nullptr;
}

}  // namespace

void InitMemoryManager(j_common_ptr cinfo) {
  MemoryManager* mem = new MemoryManager;
  mem->pub.alloc_small = jpegli::Alloc;
  mem->pub.alloc_large = jpegli::Alloc;
  mem->pub.alloc_sarray = jpegli::Alloc2dArray<JSAMPLE>;
  mem->pub.alloc_barray = jpegli::Alloc2dArray<JBLOCK>;
  mem->pub.request_virt_sarray =
      jpegli::RequestVirtualArray<jvirt_sarray_control, JSAMPLE>;
  mem->pub.request_virt_barray =
      jpegli::RequestVirtualArray<jvirt_barray_control, JBLOCK>;
  mem->pub.realize_virt_arrays = jpegli::RealizeVirtualArrays;
  mem->pub.access_virt_sarray =
      jpegli::AccessVirtualArray<jvirt_sarray_control, JSAMPLE>;
  mem->pub.access_virt_barray =
      jpegli::AccessVirtualArray<jvirt_barray_control, JBLOCK>;
  mem->pub.free_pool = jpegli::FreePool;
  mem->pub.self_destruct = jpegli::SelfDestruct;
  mem->owned_ptrs.resize(JPOOL_NUMPOOLS);
  cinfo->mem = reinterpret_cast<struct jpeg_memory_mgr*>(mem);
}

}  // namespace jpegli
