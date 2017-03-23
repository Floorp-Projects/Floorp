// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_HANDLE_H_
#define BASE_MEMORY_SHARED_MEMORY_HANDLE_H_

#include <stddef.h>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/process/process_handle.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach.h>
#include "base/base_export.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#elif defined(OS_POSIX)
#include <sys/types.h>
#include "base/file_descriptor_posix.h"
#endif

namespace base {

// SharedMemoryHandle is a platform specific type which represents
// the underlying OS handle to a shared memory segment.
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
typedef FileDescriptor SharedMemoryHandle;
#elif defined(OS_WIN)
class BASE_EXPORT SharedMemoryHandle {
 public:
  // The default constructor returns an invalid SharedMemoryHandle.
  SharedMemoryHandle();
  SharedMemoryHandle(HANDLE h, base::ProcessId pid);

  // Standard copy constructor. The new instance shares the underlying OS
  // primitives.
  SharedMemoryHandle(const SharedMemoryHandle& handle);

  // Standard assignment operator. The updated instance shares the underlying
  // OS primitives.
  SharedMemoryHandle& operator=(const SharedMemoryHandle& handle);

  // Comparison operators.
  bool operator==(const SharedMemoryHandle& handle) const;
  bool operator!=(const SharedMemoryHandle& handle) const;

  // Closes the underlying OS resources.
  void Close() const;

  // Whether the underlying OS primitive is valid.
  bool IsValid() const;

  // Whether |pid_| is the same as the current process's id.
  bool BelongsToCurrentProcess() const;

  // Whether handle_ needs to be duplicated into the destination process when
  // an instance of this class is passed over a Chrome IPC channel.
  bool NeedsBrokering() const;

  void SetOwnershipPassesToIPC(bool ownership_passes);
  bool OwnershipPassesToIPC() const;

  HANDLE GetHandle() const;
  base::ProcessId GetPID() const;

 private:
  HANDLE handle_;

  // The process in which |handle_| is valid and can be used. If |handle_| is
  // invalid, this will be kNullProcessId.
  base::ProcessId pid_;

  // Whether passing this object as a parameter to an IPC message passes
  // ownership of |handle_| to the IPC stack. This is meant to mimic the
  // behavior of the |auto_close| parameter of FileDescriptor. This member only
  // affects attachment-brokered SharedMemoryHandles.
  // Defaults to |false|.
  bool ownership_passes_to_ipc_;
};
#else
class BASE_EXPORT SharedMemoryHandle {
 public:
  // The default constructor returns an invalid SharedMemoryHandle.
  SharedMemoryHandle();

  // Makes a Mach-based SharedMemoryHandle of the given size. On error,
  // subsequent calls to IsValid() return false.
  explicit SharedMemoryHandle(mach_vm_size_t size);

  // Makes a Mach-based SharedMemoryHandle from |memory_object|, a named entry
  // in the task with process id |pid|. The memory region has size |size|.
  SharedMemoryHandle(mach_port_t memory_object,
                     mach_vm_size_t size,
                     base::ProcessId pid);

  // Standard copy constructor. The new instance shares the underlying OS
  // primitives.
  SharedMemoryHandle(const SharedMemoryHandle& handle);

  // Standard assignment operator. The updated instance shares the underlying
  // OS primitives.
  SharedMemoryHandle& operator=(const SharedMemoryHandle& handle);

  // Duplicates the underlying OS resources.
  SharedMemoryHandle Duplicate() const;

  // Comparison operators.
  bool operator==(const SharedMemoryHandle& handle) const;
  bool operator!=(const SharedMemoryHandle& handle) const;

  // Whether the underlying OS primitive is valid. Once the SharedMemoryHandle
  // is backed by a valid OS primitive, it becomes immutable.
  bool IsValid() const;

  // Exposed so that the SharedMemoryHandle can be transported between
  // processes.
  mach_port_t GetMemoryObject() const;

  // Returns false on a failure to determine the size. On success, populates the
  // output variable |size|. Returns 0 if the handle is invalid.
  bool GetSize(size_t* size) const;

  // The SharedMemoryHandle must be valid.
  // Returns whether the SharedMemoryHandle was successfully mapped into memory.
  // On success, |memory| is an output variable that contains the start of the
  // mapped memory.
  bool MapAt(off_t offset, size_t bytes, void** memory, bool read_only);

  // Closes the underlying OS primitive.
  void Close() const;

  void SetOwnershipPassesToIPC(bool ownership_passes);
  bool OwnershipPassesToIPC() const;

 private:
  // Shared code between copy constructor and operator=.
  void CopyRelevantData(const SharedMemoryHandle& handle);

  mach_port_t memory_object_ = MACH_PORT_NULL;

  // The size of the shared memory region when |type_| is MACH. Only
  // relevant if |memory_object_| is not |MACH_PORT_NULL|.
  mach_vm_size_t size_ = 0;

  // The pid of the process in which |memory_object_| is usable. Only
  // relevant if |memory_object_| is not |MACH_PORT_NULL|.
  base::ProcessId pid_ = 0;

  // Whether passing this object as a parameter to an IPC message passes
  // ownership of |memory_object_| to the IPC stack. This is meant to mimic
  // the behavior of the |auto_close| parameter of FileDescriptor.
  // Defaults to |false|.
  bool ownership_passes_to_ipc_ = false;
};
#endif

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_HANDLE_H_
