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

// Graphics memory buffer shared with all child processes.
void* gGraphicsMemory;

static mach_port_t gGraphicsPort;
static ReceivePort* gGraphicsReceiver;

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
  gGraphicsReceiver = new ReceivePort(nsPrintfCString("WebReplay.%d", getpid()).get());
}

void
SendGraphicsMemoryToChild()
{
  MachReceiveMessage handshakeMessage;
  kern_return_t kr = gGraphicsReceiver->WaitForMessage(&handshakeMessage, 0);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  MOZ_RELEASE_ASSERT(handshakeMessage.GetMessageID() == GraphicsHandshakeMessageId);
  mach_port_t childPort = handshakeMessage.GetTranslatedPort(0);
  MOZ_RELEASE_ASSERT(childPort != MACH_PORT_NULL);

  MachSendMessage message(GraphicsMemoryMessageId);
  message.AddDescriptor(MachMsgPortDescriptor(gGraphicsPort, MACH_MSG_TYPE_COPY_SEND));

  MachPortSender sender(childPort);
  kr = sender.SendMessage(message, 1000);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);
}

static Maybe<PaintMessage> gLastPaint;

// Global object for the sandbox used to paint graphics data in this process.
static JS::PersistentRootedObject* gGraphicsSandbox;

static void
InitGraphicsSandbox()
{
  MOZ_RELEASE_ASSERT(!gGraphicsSandbox);

  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::PrivilegedJunkScope())) {
    MOZ_CRASH("InitGraphicsSandbox");
  }

  JSContext* cx = jsapi.cx();

  xpc::SandboxOptions options;
  options.sandboxName.AssignLiteral("Record/Replay Graphics Sandbox");
  options.invisibleToDebugger = true;
  RootedValue v(cx);
  nsresult rv = CreateSandboxObject(cx, &v, nsXPConnect::SystemPrincipal(), options);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  gGraphicsSandbox = new JS::PersistentRootedObject(cx);
  *gGraphicsSandbox = ::js::UncheckedUnwrap(&v.toObject());

  JSAutoRealm ac(cx, *gGraphicsSandbox);

  ErrorResult er;
  dom::GlobalObject global(cx, *gGraphicsSandbox);
  RootedObject obj(cx);
  dom::ChromeUtils::Import(global, NS_LITERAL_STRING("resource://devtools/server/actors/replay/graphics.js"),
                           dom::Optional<HandleObject>(), &obj, er);
  MOZ_RELEASE_ASSERT(!er.Failed());
}

// Buffer used to transform graphics memory, if necessary.
static void* gBufferMemory;

void
UpdateGraphicsInUIProcess(const PaintMessage* aMsg)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (aMsg) {
    gLastPaint = Some(*aMsg);
  } else if (!gLastPaint.isSome()) {
    return;
  }

  // Make sure there is a sandbox which is running the graphics JS module.
  if (!gGraphicsSandbox) {
    InitGraphicsSandbox();
  }

  AutoSafeJSContext cx;
  JSAutoRealm ac(cx, *gGraphicsSandbox);

  size_t width = gLastPaint.ref().mWidth;
  size_t height = gLastPaint.ref().mHeight;
  size_t stride = layers::ImageDataSerializer::ComputeRGBStride(gSurfaceFormat, width);

  // Make sure the width and height are appropriately sized.
  CheckedInt<size_t> scaledWidth = CheckedInt<size_t>(width) * 4;
  CheckedInt<size_t> scaledHeight = CheckedInt<size_t>(height) * stride;
  MOZ_RELEASE_ASSERT(scaledWidth.isValid() && scaledWidth.value() <= stride);
  MOZ_RELEASE_ASSERT(scaledHeight.isValid() && scaledHeight.value() <= GraphicsMemorySize);

  // Get memory which we can pass to the graphics module to store in a canvas.
  // Use the shared memory buffer directly, unless we need to transform the
  // data due to extra memory in each row of the data which the child process
  // sent us.
  MOZ_RELEASE_ASSERT(gGraphicsMemory);
  void* memory = gGraphicsMemory;
  if (stride != width * 4) {
    if (!gBufferMemory) {
      gBufferMemory = malloc(GraphicsMemorySize);
    }
    memory = gBufferMemory;
    for (size_t y = 0; y < height; y++) {
      char* src = (char*)gGraphicsMemory + y * stride;
      char* dst = (char*)gBufferMemory + y * width * 4;
      memcpy(dst, src, width * 4);
    }
  }

  JSObject* bufferObject =
    JS_NewArrayBufferWithExternalContents(cx, width * height * 4, memory);
  MOZ_RELEASE_ASSERT(bufferObject);

  JS::AutoValueArray<3> args(cx);
  args[0].setObject(*bufferObject);
  args[1].setInt32(width);
  args[2].setInt32(height);

  // Call into the graphics module to update the canvas it manages.
  RootedValue rval(cx);
  if (!JS_CallFunctionName(cx, *gGraphicsSandbox, "Update", args, &rval)) {
    MOZ_CRASH("UpdateGraphicsInUIProcess");
  }
}

} // namespace parent
} // namespace recordreplay
} // namespace mozilla
