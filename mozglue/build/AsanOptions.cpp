/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#ifndef _MSC_VER  // Not supported by clang-cl yet

// When running with AddressSanitizer, we need to explicitly set some
// options specific to our codebase to prevent errors during runtime.
// To override these, set the ASAN_OPTIONS environment variable.
//
// Currently, these are:
//
//   allow_user_segv_handler=1 - Tell ASan to allow our code to use its
//   own SIGSEGV handlers. This is required by ASM.js internally.
//
//   alloc_dealloc_mismatch=0 - Disable alloc-dealloc mismatch checking
//   in ASan. This is required because we define our own new/delete
//   operators that are backed by malloc/free. If one of them gets inlined
//   while the other doesn't, ASan will report false positives.
//
//   detect_leaks=0 - Disable LeakSanitizer. This is required because
//   otherwise leak checking will be enabled for various building and
//   testing executables where we don't care much about leaks.
//
//   allocator_may_return_null=1 - Tell ASan to return NULL when an allocation
//   fails instead of aborting the program. This allows us to handle failing
//   allocations the same way we would handle them with a regular allocator and
//   also uncovers potential bugs that might occur in these situations.
//
//   max_malloc_fill_size - Tell ASan to initialize memory to a certain value
//   when it is allocated. This option specifies the maximum allocation size
//   for which ASan should still initialize the memory. The value we specify
//   here is exactly 256MiB.
//
//   max_free_fill_size - Similar to max_malloc_fill_size, tell ASan to
//   overwrite memory with a certain value when it is freed. Again, the value
//   here specifies the maximum allocation size, larger allocations will
//   skipped.
//
//   malloc_fill_byte / free_fill_byte - These values specify the byte values
//   used to initialize/overwrite memory in conjunction with the previous
//   options max_malloc_fill_size and max_free_fill_size. The values used here
//   are 0xe4 and 0xe5 to match the kAllocPoison and kAllocJunk constants used
//   by mozjemalloc.
//
//   malloc_context_size - This value specifies how many stack frames are
//   stored for each malloc and free call. Since Firefox can have lots of deep
//   stacks with allocations, we limit the default size here further to save
//   some memory.
//
//   fast_unwind_on_check - Use the fast (frame-pointer-based) stack unwinder
//   for internal CHECK failures. The slow unwinder doesn't work on Android.
//
//   fast_unwind_on_fatal - Use the fast (frame-pointer-based) stack unwinder
//   to print fatal error reports. The slow unwinder doesn't work on Android.
//
//   detect_stack_use_after_return=0 - Work around bug 1768099.
//
//   intercept_tls_get_addr=0 - Work around
//   https://github.com/google/sanitizers/issues/1322 (bug 1635327).
//
// !! Note: __asan_default_options is not used on Android! (bug 1576213)
// These should be updated in:
//   mobile/android/geckoview/src/asan/resources/lib/*/wrap.sh
//
extern "C" MOZ_ASAN_BLACKLIST const char* __asan_default_options() {
  return "allow_user_segv_handler=1:alloc_dealloc_mismatch=0:detect_leaks=0"
#  ifdef MOZ_ASAN_REPORTER
         ":malloc_context_size=20"
#  endif
#  ifdef __ANDROID__
         ":fast_unwind_on_check=1:fast_unwind_on_fatal=1"
#  endif
         ":max_free_fill_size=268435456:max_malloc_fill_size=268435456"
         ":malloc_fill_byte=228:free_fill_byte=229"
         ":handle_sigill=1"
         ":allocator_may_return_null=1"
         ":detect_stack_use_after_return=0"
         ":intercept_tls_get_addr=0";
}

// !!! Please do not add suppressions for new leaks in Gecko code, unless they
// are intentional !!!
extern "C" const char* __lsan_default_suppressions() {
  return "# Add your suppressions below\n"

         // LSan runs with a shallow stack depth and no debug symbols, so some
         // small intentional leaks in system libraries show up with this.  You
         // do not want this enabled when running locally with a deep stack, as
         // it can catch too much.
         "leak:libc.so\n"

         // nsComponentManagerImpl intentionally leaks factory entries, and
         // probably some other stuff.
         "leak:nsComponentManagerImpl\n"

         // Bug 981220 - Pixman fails to free TLS memory.
         "leak:pixman_implementation_lookup_composite\n"

         // Bug 987918 - Font shutdown leaks when CLEANUP_MEMORY is not enabled.
         "leak:libfontconfig.so\n"
         "leak:libfreetype.so\n"
         "leak:GI___strdup\n"
         // The symbol is really __GI___strdup, but if you have the leading _,
         // it doesn't suppress it.

         // Bug 1078015 - If the process terminates during a PR_Sleep, LSAN
         // detects a leak
         "leak:PR_Sleep\n"

         // Bug 1363976 - Stylo holds some global data alive forever.
         "leak:style::global_style_data\n"
         "leak:style::sharing::SHARING_CACHE_KEY\n"
         "leak:style::bloom::BLOOM_KEY\n"

         //
         // Many leaks only affect some test suites.  The suite annotations are
         // not checked.
         //

         // Bug 979928 - WebRTC leaks in different mochitest suites.
         "leak:NR_reg_init\n"
         // nr_reg_local_init should be redundant with NR_reg_init, but on
         // Aurora we get fewer stack frames for some reason.
         "leak:nr_reg_local_init\n"
         "leak:r_log_register\n"
         "leak:nr_reg_set\n"

         // This is a one-time leak in mochitest-bc, so it is probably okay to
         // ignore.
         "leak:GlobalPrinters::InitializeGlobalPrinters\n"
         "leak:nsPSPrinterList::GetPrinterList\n"

         // Bug 1028456 - Various NSPR fd-related leaks in different mochitest
         // suites.
         "leak:_PR_Getfd\n"

         // Bug 1028483 - The XML parser sometimes leaks an object. Mostly
         // happens in toolkit/components/thumbnails.
         "leak:processInternalEntity\n"

         // Bug 1187421 - NSS does not always free the error stack in different
         // mochitest suites.
         "leak:nss_ClearErrorStack\n"

         // Bug 1602689 - leak at mozilla::NotNull, RacyRegisteredThread,
         // RegisteredThread::RegisteredThread, mozilla::detail::UniqueSelector
         "leak:RegisteredThread::RegisteredThread\n"

         //
         // Leaks with system libraries in their stacks. These show up across a
         // number of tests. Better symbols and disabling fast stackwalking may
         // help diagnose these.
         //
         "leak:libcairo.so\n"
         // https://github.com/OpenPrinting/cups/pull/317
         "leak:libcups.so\n"
         "leak:libdl.so\n"
         "leak:libdricore.so\n"
         "leak:libdricore9.2.1.so\n"
         "leak:libGL.so\n"
         "leak:libEGL_mesa.so\n"
         "leak:libglib-2.0.so\n"
         "leak:libglsl.so\n"
         "leak:libp11-kit.so\n"
         "leak:libpixman-1.so\n"
         "leak:libpulse.so\n"
         // lubpulsecommon 1.1 is Ubuntu 12.04
         "leak:libpulsecommon-1.1.so\n"
         // lubpulsecommon 1.1 is Ubuntu 16.04
         "leak:libpulsecommon-8.0.so\n"
         "leak:libresolv.so\n"
         "leak:libstdc++.so\n"
         "leak:libXrandr.so\n"
         "leak:libX11.so\n"
         "leak:pthread_setspecific_internal\n"
         "leak:swrast_dri.so\n"

         "leak:js::frontend::BytecodeEmitter:\n"
         "leak:js::frontend::GeneralParser\n"
         "leak:js::frontend::Parse\n"
         "leak:xpc::CIGSHelper\n"
         "leak:mozJSModuleLoader\n"
         "leak:mozilla::xpcom::ConstructJSMComponent\n"
         "leak:XPCWrappedNativeJSOps\n"

      // End of suppressions.
      ;  // Please keep this semicolon.
}

#endif  // _MSC_VER
