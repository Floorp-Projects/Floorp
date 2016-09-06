// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/resolver.h"

#include <stddef.h>

#include "base/win/pe_image.h"
#include "sandbox/win/src/sandbox_nt_util.h"

namespace sandbox {

NTSTATUS ResolverThunk::Init(const void* target_module,
                             const void* interceptor_module,
                             const char* target_name,
                             const char* interceptor_name,
                             const void* interceptor_entry_point,
                             void* thunk_storage,
                             size_t storage_bytes) {
  if (NULL == thunk_storage || 0 == storage_bytes ||
      NULL == target_module || NULL == target_name)
    return STATUS_INVALID_PARAMETER;

  if (storage_bytes < GetThunkSize())
    return STATUS_BUFFER_TOO_SMALL;

  NTSTATUS ret = STATUS_SUCCESS;
  if (NULL == interceptor_entry_point) {
    ret = ResolveInterceptor(interceptor_module, interceptor_name,
                             &interceptor_entry_point);
    if (!NT_SUCCESS(ret))
      return ret;
  }

  ret = ResolveTarget(target_module, target_name, &target_);
  if (!NT_SUCCESS(ret))
    return ret;

  interceptor_ = interceptor_entry_point;

  return ret;
}

NTSTATUS ResolverThunk::ResolveInterceptor(const void* interceptor_module,
                                           const char* interceptor_name,
                                           const void** address) {
  DCHECK_NT(address);
  if (!interceptor_module)
    return STATUS_INVALID_PARAMETER;

  base::win::PEImage pe(interceptor_module);
  if (!pe.VerifyMagic())
    return STATUS_INVALID_IMAGE_FORMAT;

  *address = pe.GetProcAddress(interceptor_name);

  if (!(*address))
    return STATUS_PROCEDURE_NOT_FOUND;

  return STATUS_SUCCESS;
}

}  // namespace sandbox
