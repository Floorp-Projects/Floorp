// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SYNC_INTERCEPTION_H_
#define SANDBOX_WIN_SRC_SYNC_INTERCEPTION_H_

#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

extern "C" {

typedef NTSTATUS(WINAPI* NtCreateEventFunction)(
    PHANDLE EventHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    EVENT_TYPE EventType,
    BOOLEAN InitialState);

typedef NTSTATUS(WINAPI* NtOpenEventFunction)(
    PHANDLE EventHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes);

// Interceptors for NtCreateEvent/NtOpenEvent
SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtCreateEvent(NtCreateEventFunction orig_CreateEvent,
                    PHANDLE event_handle,
                    ACCESS_MASK desired_access,
                    POBJECT_ATTRIBUTES object_attributes,
                    EVENT_TYPE event_type,
                    BOOLEAN initial_state);

SANDBOX_INTERCEPT NTSTATUS WINAPI
TargetNtOpenEvent(NtOpenEventFunction orig_OpenEvent,
                  PHANDLE event_handle,
                  ACCESS_MASK desired_access,
                  POBJECT_ATTRIBUTES object_attributes);

}  // extern "C"

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SYNC_INTERCEPTION_H_
