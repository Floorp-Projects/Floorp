// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/top_level_dispatcher.h"

#include <stdint.h>
#include <string.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/filesystem_dispatcher.h"
#include "sandbox/win/src/handle_dispatcher.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/named_pipe_dispatcher.h"
#include "sandbox/win/src/process_mitigations_win32k_dispatcher.h"
#include "sandbox/win/src/process_thread_dispatcher.h"
#include "sandbox/win/src/registry_dispatcher.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sync_dispatcher.h"

namespace sandbox {

TopLevelDispatcher::TopLevelDispatcher(PolicyBase* policy) : policy_(policy) {
  // Initialize the IPC dispatcher array.
  memset(ipc_targets_, 0, sizeof(ipc_targets_));
  Dispatcher* dispatcher;

  dispatcher = new FilesystemDispatcher(policy_);
  ipc_targets_[IPC_NTCREATEFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTSETINFO_RENAME_TAG] = dispatcher;
  ipc_targets_[IPC_NTQUERYATTRIBUTESFILE_TAG] = dispatcher;
  ipc_targets_[IPC_NTQUERYFULLATTRIBUTESFILE_TAG] = dispatcher;
  filesystem_dispatcher_.reset(dispatcher);

  dispatcher = new NamedPipeDispatcher(policy_);
  ipc_targets_[IPC_CREATENAMEDPIPEW_TAG] = dispatcher;
  named_pipe_dispatcher_.reset(dispatcher);

  dispatcher = new ThreadProcessDispatcher(policy_);
  ipc_targets_[IPC_NTOPENTHREAD_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESS_TAG] = dispatcher;
  ipc_targets_[IPC_CREATEPROCESSW_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESSTOKEN_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENPROCESSTOKENEX_TAG] = dispatcher;
  thread_process_dispatcher_.reset(dispatcher);

  dispatcher = new SyncDispatcher(policy_);
  ipc_targets_[IPC_CREATEEVENT_TAG] = dispatcher;
  ipc_targets_[IPC_OPENEVENT_TAG] = dispatcher;
  sync_dispatcher_.reset(dispatcher);

  dispatcher = new RegistryDispatcher(policy_);
  ipc_targets_[IPC_NTCREATEKEY_TAG] = dispatcher;
  ipc_targets_[IPC_NTOPENKEY_TAG] = dispatcher;
  registry_dispatcher_.reset(dispatcher);

  dispatcher = new HandleDispatcher(policy_);
  ipc_targets_[IPC_DUPLICATEHANDLEPROXY_TAG] = dispatcher;
  handle_dispatcher_.reset(dispatcher);

  dispatcher = new ProcessMitigationsWin32KDispatcher(policy_);
  ipc_targets_[IPC_GDI_GDIDLLINITIALIZE_TAG] = dispatcher;
  ipc_targets_[IPC_GDI_GETSTOCKOBJECT_TAG] = dispatcher;
  ipc_targets_[IPC_USER_REGISTERCLASSW_TAG] = dispatcher;
  process_mitigations_win32k_dispatcher_.reset(dispatcher);
}

TopLevelDispatcher::~TopLevelDispatcher() {}

// When an IPC is ready in any of the targets we get called. We manage an array
// of IPC dispatchers which are keyed on the IPC tag so we normally delegate
// to the appropriate dispatcher unless we can handle the IPC call ourselves.
Dispatcher* TopLevelDispatcher::OnMessageReady(IPCParams* ipc,
                                               CallbackGeneric* callback) {
  DCHECK(callback);
  static const IPCParams ping1 = {IPC_PING1_TAG, {UINT32_TYPE}};
  static const IPCParams ping2 = {IPC_PING2_TAG, {INOUTPTR_TYPE}};

  if (ping1.Matches(ipc) || ping2.Matches(ipc)) {
    *callback = reinterpret_cast<CallbackGeneric>(
        static_cast<Callback1>(&TopLevelDispatcher::Ping));
    return this;
  }

  Dispatcher* dispatcher = GetDispatcher(ipc->ipc_tag);
  if (!dispatcher) {
    NOTREACHED();
    return NULL;
  }
  return dispatcher->OnMessageReady(ipc, callback);
}

// Delegate to the appropriate dispatcher.
bool TopLevelDispatcher::SetupService(InterceptionManager* manager,
                                      int service) {
  if (IPC_PING1_TAG == service || IPC_PING2_TAG == service)
    return true;

  Dispatcher* dispatcher = GetDispatcher(service);
  if (!dispatcher) {
    NOTREACHED();
    return false;
  }
  return dispatcher->SetupService(manager, service);
}

// We service IPC_PING_TAG message which is a way to test a round trip of the
// IPC subsystem. We receive a integer cookie and we are expected to return the
// cookie times two (or three) and the current tick count.
bool TopLevelDispatcher::Ping(IPCInfo* ipc, void* arg1) {
  switch (ipc->ipc_tag) {
    case IPC_PING1_TAG: {
      IPCInt ipc_int(arg1);
      uint32_t cookie = ipc_int.As32Bit();
      ipc->return_info.extended_count = 2;
      ipc->return_info.extended[0].unsigned_int = ::GetTickCount();
      ipc->return_info.extended[1].unsigned_int = 2 * cookie;
      return true;
    }
    case IPC_PING2_TAG: {
      CountedBuffer* io_buffer = reinterpret_cast<CountedBuffer*>(arg1);
      if (sizeof(uint32_t) != io_buffer->Size())
        return false;

      uint32_t* cookie = reinterpret_cast<uint32_t*>(io_buffer->Buffer());
      *cookie = (*cookie) * 3;
      return true;
    }
    default:
      return false;
  }
}

Dispatcher* TopLevelDispatcher::GetDispatcher(int ipc_tag) {
  if (ipc_tag >= IPC_LAST_TAG || ipc_tag <= IPC_UNUSED_TAG)
    return NULL;

  return ipc_targets_[ipc_tag];
}

}  // namespace sandbox
