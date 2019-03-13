/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// GECKO_PROCESS_TYPE(enum-name, string-name, XRE_Is${NAME}Process)
// Note that string-name is exposed to various things like telemetry
// and the crash reporter, so it should not be changed casually.
//
// The values generated for the GeckoProcessType enum are dependent on
// the ordering of the GECKO_PROCESS_TYPE invocations in this file, and
// said values are exposed to things like telemetry as well, so please
// do not reorder lines in this file.
//
// Please add new process types at the end of this list.
GECKO_PROCESS_TYPE(Default, "default", Parent)
GECKO_PROCESS_TYPE(Plugin, "plugin", Plugin)
GECKO_PROCESS_TYPE(Content, "tab", Content)
GECKO_PROCESS_TYPE(IPDLUnitTest, "ipdlunittest", IPDLUnitTest)
// Gecko Media Plugin process.
GECKO_PROCESS_TYPE(GMPlugin, "gmplugin", GMPlugin)
// GPU and compositor process.
GECKO_PROCESS_TYPE(GPU, "gpu", GPU)
// VR process.
GECKO_PROCESS_TYPE(VR, "vr", VR)
// Remote Data Decoder process.
GECKO_PROCESS_TYPE(RDD, "rdd", RDD)
// Socket process
GECKO_PROCESS_TYPE(Socket, "socket", Socket)
GECKO_PROCESS_TYPE(RemoteSandboxBroker, "sandbox", RemoteSandboxBroker)
