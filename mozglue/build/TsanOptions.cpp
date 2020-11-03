/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/TsanOptions.h"

#ifndef _MSC_VER  // Not supported by clang-cl yet

//
// When running with ThreadSanitizer, we sometimes need to suppress existing
// races. However, in any case, it should be either because
//
//    1) a bug is on file. In this case, the bug number should always be
//       included with the suppression.
//
// or 2) this is an intentional race. Please be very careful with judging
//       races as intentional and benign. Races in C++ are undefined behavior
//       and compilers increasingly rely on exploiting this for optimizations.
//       Hence, many seemingly benign races cause harmful or unexpected
//       side-effects.
//
//       See also:
//       https://software.intel.com/en-us/blogs/2013/01/06/benign-data-races-what-could-possibly-go-wrong
//
//
// Also, when adding any race suppressions here, make sure to always add
// a signature for each of the two race stacks. Sometimes, TSan fails to
// symbolize one of the two traces and this can cause suppressed races to
// show up intermittently.
//
// clang-format off
extern "C" const char* __tsan_default_suppressions() {
  return "# Add your suppressions below\n"

         // External uninstrumented libraries
         MOZ_TSAN_DEFAULT_EXTLIB_SUPPRESSIONS

         // TSan internals
         "race:__tsan::ProcessPendingSignals\n"
         "race:__tsan::CallUserSignalHandler\n"





         // Uninstrumented code causing false positives

         // These libraries are uninstrumented and cause mutex false positives.
         // However, they can be unloaded by GTK early which we cannot avoid.
         "mutex:libGL.so\n"
         "mutex:libGLdispatch\n"
         "mutex:libGLX\n"
         // Bug 1637707 - permanent
         "mutex:libEGL_mesa.so\n"
         // ~GLContextGLX unlocks a libGL mutex.
         "mutex:GLContextGLX::~GLContextGLX\n"
         // Bug 1651446 - permanent (ffmpeg)
         "race:libavcodec.so*\n"
         "race:libavutil.so*\n"
         // For some reason, the suppressions on libpulse.so
         // through `called_from_lib` only work partially.
         "race:libpulse.so\n"
         "race:pa_context_suspend_source_by_index\n"
         "race:pa_context_unref\n"
         "race:pa_format_info_set_prop_string_array\n"
         "race:pa_stream_get_index\n"
         "race:pa_stream_update_timing_info\n"
         // This is a callback from libglib-2 that is apparently
         // not fully suppressed through `called_from_lib`.
         "race:g_main_context_dispatch\n"
         // This is likely a false positive involving a mutex from GTK.
         // See also bug 1642653 - permanent.
         "mutex:GetMaiAtkType\n"





         // Deadlock reports on single-threaded runtime.
         //
         // This is a known false positive from TSan where it reports
         // a potential deadlock even though all mutexes are only
         // taken by a single thread. For applications/tasks where we
         // are absolutely sure that no second thread will be involved
         // we should suppress these issues.
         //
         // See also https://github.com/google/sanitizers/issues/488

         // Bug 1614605 - permanent
         "deadlock:SanctionsTestServer\n"
         "deadlock:OCSPStaplingServer\n"
         // Bug 1643087 - permanent
         "deadlock:BadCertAndPinningServer\n"
         // Bug 1606804 - permanent
         "deadlock:cert_storage::SecurityState::open_db\n"
         "deadlock:cert_storage::SecurityState::add_certs\n"
         // Bug 1651770 - permanent
         "deadlock:mozilla::camera::LockAndDispatch\n"
         // Bug 1606804 - permanent
         "deadlock:third_party/rust/rkv/src/env.rs\n"





         // Benign races in third-party code (should be fixed upstream)

         // No Bug - permanent
         // No Upstream Bug Filed!
         //
         // SIMD Initialization in libjpeg, potentially runs
         // initialization twice, but otherwise benign. Init
         // routine itself is in native assembler.
         "race:init_simd\n"
         "race:simd_support\n"
         "race:jsimd_can_ycc_rgb\n"
         // Bug 1615228 - permanent
         // No Upstream Bug Filed!
         //
         // Likely benign race in ipc/chromium/ where we set
         // `message_loop_` to `NULL` on two threads when stopping
         // a thread at the same time it is already finishing.
         "race:base::Thread::Stop\n"
         // Bug 1615569 - permanent
         // No Upstream Bug Filed!
         //
         // NSS is using freebl from two different threads but freebl isn't
         // that threadsafe.
         "race:mp_exptmod.max_window_bits\n"
         // Bug 1652499 - permanent
         // No Upstream Bug Filed!
         //
         // Likely benign race in webrtc.org code - race while updating the
         // minimum log severity.
         "race:Loggable\n"
         "race:UpdateMinLogSeverity\n"
         // Bug 1652174 - permanent
         // Upstream Bug: https://github.com/libevent/libevent/issues/777
         //
         // Likely benign write-write race in libevent to set a sticky boolean
         // flag to true.
         "race:event_debug_mode_too_late\n"
         // Bug 1648606 - permanent
         // No Upstream Bug Filed!
         //
         // Race on some flag being checking in libusrsctp.
         "race:sctp_close\n"
         "race:sctp_iterator_work\n"
         // Bug 1653618 - permanent
         // Upstream Bug: https://github.com/sctplab/usrsctp/issues/507
         //
         // Might lead to scheduled timers in libusrsctp getting dropped?
         "race:sctp_handle_tick\n"
         "race:sctp_handle_sack\n"
         // Bug 1648604 - permanent
         // Upstream Bug: https://github.com/sctplab/usrsctp/issues/482
         //
         // Likely benign race in libusrsctp allocator during a free.
         "race:system_base_info\n"
         // Bug 1153409 - permanent
         // No Upstream Bug Filed!
         //
         // Probably benign - sqlite has a few optimizations where it does
         // racy reads and then does properly synchornized integrity checks
         // afterwards. Some concern of compiler optimizations messing this
         // up due to "volatile" being too weak for this.
         "race:third_party/sqlite3/*\n"
         "deadlock:third_party/sqlite3/*\n"





         // Lack of proper instrumentation for the Rust stdlib (fix this!).
         //
         // All of these can potentially be removed if we fix Bug 1671691.

         // Bug 1587513 - permanent
         "race:std::sync::mutex::Mutex\n"
         // Bug 1590423 - permanent
         "race:sync..Arc\n"
         "race:alloc::sync::Arc\n"
         // No Bug - permanent
         "race:third_party/rust/parking_lot_core/*\n"
         // No Bug - permanent
         "race:/rustc/*.rs\n"
         "deadlock:/rustc/*.rs\n"
         "thread:std::sys::unix::thread::Thread::new\n"





         // Benign read/write races on bitfields
         //
         // WARNING: Bitfield races are only benign if one of the concurrent
         // accesses is a read. Write/write races on different parts of a
         // bitfield can have severe side-effects.
         //
         // These should all still be fixed because the compiler is incentivized
         // to combine/cache these accesses without proper atomic annotations.

         // No Bug
         "race:WalkDiskCacheRunnable::Run\n"
         // No Bug - Modifying `mResolveAgain` while reading `mGetTtl`
         "race:RemoveOrRefresh\n"
         "race:nsHostResolver::ThreadFunc\n"
         // Bug 1614697
         "race:nsHttpChannel::OnCacheEntryCheck\n"
         "race:~AutoCacheWaitFlags\n"





         // The rest of these suppressions are miscellaneous issues in gecko
         // that should be investigated and ideally fixed.

         // Bug 1619162
         "race:currentNameHasEscapes\n"

         // Bug 1601600
         "race:SkARGB32_Blitter\n"
         "race:SkARGB32_Shader_Blitter\n"
         "race:SkARGB32_Opaque_Blitter\n"
         "race:SkRasterPipelineBlitter\n"
         "race:Clamp_S32_D32_nofilter_trans_shaderproc\n"
         "race:SkSpriteBlitter_Memcpy\n"

         // Bug 1601632
         "race:ScriptPreloader::MaybeFinishOffThreadDecode\n"
         "race:ScriptPreloader::DoFinishOffThreadDecode\n"

         // Bug 1606651
         "race:nsPluginTag::nsPluginTag\n"
         "race:nsFakePluginTag\n"

         // Bug 1606800
         "race:CallInitFunc\n"

         // Bug 1606864
         "race:nsSocketTransport::Close\n"
         "race:nsSocketTransport::OnSocketDetached\n"

         // Bug 1607138
         "race:gXPCOMThreadsShutDown\n"

         // Bug 1607446
         "race:nsJARChannel::Suspend\n"
         "race:nsJARChannel::Resume\n"

         // Bug 1607449
         "race:fill_CERTCertificateFields\n"
         "race:CERT_DestroyCertificate\n"

         // Bug 1608462
         "deadlock:ScriptPreloader::OffThreadDecodeCallback\n"

         // Bug 1615017
         "race:CacheFileMetadata::SetHash\n"
         "race:CacheFileMetadata::OnDataWritten\n"

         // Bug 1615123
         "race:_dl_deallocate_tls\n"
         "race:__libc_memalign\n"

         // Bug 1664535
         "race:setNeedsIncrementalBarrier\n"
         "race:needsIncrementalBarrier\n"

         // Bug 1664803
         "race:Sampler::sSigHandlerCoordinator\n"

         // Bug 1656068
         "race:WebRtcAec_Create\n"

         // No Bug - Logging bug in Mochitests
         "race:mochitest/ssltunnel/ssltunnel.cpp\n"

         // No Bug - Suppress thread leaks for now
         "thread:NS_NewNamedThread\n"
         "thread:nsThread::Init\n"
         "thread:libglib-2\n"

         // No Bug - This thread does not seem to be stopped/joined
         "thread:mozilla::layers::ImageBridgeChild\n"
         "race:mozilla::layers::ImageBridgeChild::ShutDown\n"

         // Bug 1652530
         "mutex:XErrorTrap\n"

         // Bug 1671572
         "race:IdentifyTextureHost\n"
         "race:GetCompositorBackendType\n"
         "race:SupportsTextureDirectMapping\n"

         // Bug 1671574
         "thread:StartupCache\n"

         // Bug 1671601
         "race:CamerasParent::ActorDestroy\n"
         "race:CamerasParent::DispatchToVideoCaptureThread\n"

         // Bug 1672230
         "race:ScriptPreloader::Trace\n"
         "race:ScriptPreloader::WriteCache\n"

      // End of suppressions.
      ;  // Please keep this semicolon.
}
// clang-format on
#endif  // _MSC_VER
