/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the middleman process uses to send messages to
// the UI process with painting data from the child process.

#include "ParentInternal.h"

#include "chrome/common/mach_ipc_mac.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/PTextureChild.h"

#include <mach/mach_vm.h>

namespace mozilla {
namespace recordreplay {
namespace parent {

void* gGraphicsMemory;
mach_port_t gGraphicsPort;

void
InitializeGraphicsMemory()
{
  mach_vm_address_t address;
  kern_return_t kr = mach_vm_allocate(mach_task_self(), &address,
                                      GraphicsMemorySize, VM_FLAGS_ANYWHERE);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  memory_object_size_t memoryObjectSize = GraphicsMemorySize;
  kr = mach_make_memory_entry_64(mach_task_self(),
                                 &memoryObjectSize,
                                 address,
                                 VM_PROT_DEFAULT,
                                 &gGraphicsPort,
                                 MACH_PORT_NULL);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);
  MOZ_RELEASE_ASSERT(memoryObjectSize == GraphicsMemorySize);

  gGraphicsMemory = (void*) address;
}

} // namespace parent
} // namespace recordreplay
} // namespace mozilla
