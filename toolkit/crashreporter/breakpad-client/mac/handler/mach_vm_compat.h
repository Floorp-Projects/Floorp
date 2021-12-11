// Copyright (c) 2011, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CLIENT_MAC_GENERATOR_MACH_VM_COMPAT_H_
#define CLIENT_MAC_GENERATOR_MACH_VM_COMPAT_H_

#include <TargetConditionals.h>

// On iOS 5 and higher, mach/mach_vm.h is not supported. Use the corresponding
// vm_map functions instead.
#if TARGET_OS_IPHONE
#include <mach/vm_map.h>
#define mach_vm_address_t vm_address_t
#define mach_vm_deallocate vm_deallocate
#define mach_vm_read vm_read
#define mach_vm_region_recurse vm_region_recurse_64
#define mach_vm_size_t vm_size_t
#else
#include <mach/mach_vm.h>
#endif  // TARGET_OS_IPHONE

// This is the current (as of macOS 11.2.3) raw format of the data in the
// __crash_info section of the __DATA segment. It must be read from memory
// (__crash_info sections in file system modules are always empty of useful
// data). The data (and that of its strings) may be readable from an out-of-
// process image, but generally only if the module is in the dyld shared
// cache. A __crash_info section is considered "empty of useful data" if all
// of its fields besides 'version' are zero. The __crash_info section is only
// present on macOS. Apple may add new fields to this structure in the future,
// as they added 'abort_cause' when 'version' changed from '4' to '5'.
// Breakpad should take this into account, and provide a way for new fields to
// be added to future versions of MDRawMacCrashInfoRecord without breaking
// older code.
typedef struct {                // Non-raw format
  uint64_t version;             // unsigned long
  uint64_t message;             // char *
  uint64_t signature_string;    // char *
  uint64_t backtrace;           // char *
  uint64_t message2;            // char *
  // We can't use uint64_t for 'thread' in non-raw structures: Though uint64_t
  // is the same size on all platforms, it's "unsigned long" on some platforms
  // (like Android and Linux) and "unsigned long long" on others (like macOS
  // and Windows).
  uint64_t thread;              // uint64_t, but we use unsigned long long
  uint64_t dialog_mode;         // unsigned int
  // There's a string in the OSAnalytics private framework ("Abort Cause %lld")
  // which hints that this field's non-raw format is long long.
  uint64_t abort_cause;         // long long, only present when
                                //   'version' > 4
} crashreporter_annotations_t;

// Defines for calls to getsectdatafromheader_64() or getsectdatafromheader().
// __LP64__ is true on both amd64 and arm64 (Apple Silicon) hardware.
#ifdef __LP64__
  #define getsectdatafromheader_func getsectdatafromheader_64
  typedef uint64_t getsectdata_size_type;
#else
  #define getsectdatafromheader_func getsectdatafromheader
  typedef uint32_t getsectdata_size_type;
#endif

#endif  // CLIENT_MAC_GENERATOR_MACH_VM_COMPAT_H_
