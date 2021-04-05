/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The entries in this file define support functions for each of the process
// types present in Gecko.  The format is:
//
// GECKO_PROCESS_TYPE(enum-value, enum-name, string-name, proc-typename,
// process-bin-type)
//
// enum-value: Unsigned int value the enum will use to identify the process
// type.  This value must not be shared by different process types and should
// never be changed since it is used e.g. in telemetry reporting.  These
// values should be mirrored in nsIXULRuntime.idl.
//
// enum-name: used to name the GeckoChildProcess enum.  E.g. `Foo` will
// become `GeckoChildProcess_Foo`.  The enum's value will be the enum-value
// above.
//
// string-name: Human-readable name.  It is exposed to things like
// telemetry and the crash reporter, so it should not be changed casually.
//
// proc-typename: Used as NAME in the `XRE_Is${NAME}Process` function.
// Ideally, this should match the enum-name.  This is included since there
// are legacy exceptions to that rule.
//
// process-bin-type: either Self or PluginContainer.  Determines
// whether the child process may be started using the same binary as the parent
// process, or whether to use plugin-container (Note that whether or not this
// value is actually obeyed depends on platform and build configuration. Do not
// use this value directly, but rather use XRE_GetChildProcBinPathType to
// resolve this).

GECKO_PROCESS_TYPE(0, Default, "default", Parent, Self)
GECKO_PROCESS_TYPE(2, Content, "tab", Content, Self)
GECKO_PROCESS_TYPE(3, IPDLUnitTest, "ipdlunittest", IPDLUnitTest,
                   PluginContainer)
// Gecko Media Plugin process.
GECKO_PROCESS_TYPE(4, GMPlugin, "gmplugin", GMPlugin, PluginContainer)
// GPU and compositor process.
GECKO_PROCESS_TYPE(5, GPU, "gpu", GPU, Self)
// VR process.
GECKO_PROCESS_TYPE(6, VR, "vr", VR, Self)
// Remote Data Decoder process.
GECKO_PROCESS_TYPE(7, RDD, "rdd", RDD, Self)
// Socket process
GECKO_PROCESS_TYPE(8, Socket, "socket", Socket, Self)
GECKO_PROCESS_TYPE(9, RemoteSandboxBroker, "sandboxbroker", RemoteSandboxBroker,
                   PluginContainer)
GECKO_PROCESS_TYPE(10, ForkServer, "forkserver", ForkServer, Self)

// Please add new process types at the end of this list.  You will also need
// to maintain consistency with:
//
// * toolkit/components/processtools/ProcInfo.h (ProcType),
// * xpcom/system/nsIXULRuntime.idl (PROCESS_TYPE constants),
// * toolkit/xre/nsAppRunner.cpp (SYNC_ENUMS),
// * dom/base/ChromeUtils.cpp (ProcTypeToWebIDL and
//   ChromeUtils::RequestProcInfo)
// * dom/chrome-webidl/ChromeUtils.webidl (WebIDLProcType)
// * toolkit/components/crashes/nsICrashService.idl and
//   CrashService.jsm (PROCESS_TYPE constants)
// * ipc/glue/CrashReporterHost.cpp (assertions)
//
// Also, please ensure that any new sandbox environment variables are added
// in build/pgo/profileserver.py to ensure your new process participates in
// PGO profile generation.
