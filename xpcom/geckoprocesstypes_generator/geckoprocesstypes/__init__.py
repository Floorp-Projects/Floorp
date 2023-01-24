# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import namedtuple

# Please make sure you follow ipc/docs/processes.rst and keep updating all
# places that might depend on a new process type and that are not covered (yet)
# by that code.

# The entries in this file define support functions for each of the process
# types present in Gecko.
#
# GECKO_PROCESS_TYPE(
#   enum-value,
#   enum-name,
#   string-name,
#   proc-typename,
#   process-bin-type,
#   procinfo-typename,
#   webidl-typename,
#   allcaps-name,
#   crash-ping,
# )
#
# enum-value:
#   Unsigned int value the enum will use to identify the process type.  This
#   value must not be shared by different process types and should never be
#   changed since it is used e.g. in telemetry reporting.
#
#   ***These values should be mirrored in nsIXULRuntime.idl.***
#
# enum-name:
#   Used to name the GeckoChildProcess enum.  E.g. `Foo` will become
#   `GeckoChildProcess_Foo`.  The enum's value will be the enum-value above.
#
# string-name:
#   Human-readable name.  It is exposed to things like telemetry and the crash
#   reporter, so it should not be changed casually.
#
# proc-typename:
#   Used as NAME in the `XRE_Is${NAME}Process` function.  Ideally, this should
#   match the enum-name.  This is included since there are legacy exceptions to
#   that rule.
#
# process-bin-type:
#   Either Self or PluginContainer.  Determines whether the child process may
#   be started using the same binary as the parent process, or whether to use
#   plugin-container (Note that whether or not this value is actually obeyed
#   depends on platform and build configuration. Do not use this value
#   directly, but rather use XRE_GetChildProcBinPathType to resolve this).
#
# procinfo-typename:
#   Used as NAME in the ProcType enum defined by ProcInfo.h.
#
# webidl-typename:
#   Used as NAME in the ChromeUtils.cpp code
#
# allcaps-name:
#   Used as NAME for checking SYNC_ENUM in nsXULAppAPI.h
#
# crash-ping:
#   Boolean reflecting if the process is allowed to send crash ping.


GeckoProcessType = namedtuple(
    "GeckoProcessType",
    [
        "enum_value",
        "enum_name",
        "string_name",
        "proc_typename",
        "process_bin_type",
        "procinfo_typename",
        "webidl_typename",
        "allcaps_name",
        "crash_ping",
    ],
)

process_types = [
    GeckoProcessType(
        0,
        "Default",
        "default",
        "Parent",
        "Self",
        "Browser",
        "Browser",
        "DEFAULT",
        True,
    ),
    GeckoProcessType(
        2,
        "Content",
        "tab",
        "Content",
        "Self",
        "Content",
        "Content",
        "CONTENT",
        True,
    ),
    GeckoProcessType(
        3,
        "IPDLUnitTest",
        "ipdlunittest",
        "IPDLUnitTest",
        "Self",
        "IPDLUnitTest",
        "IpdlUnitTest",
        "IPDLUNITTEST",
        False,
    ),
    GeckoProcessType(
        4,
        "GMPlugin",
        "gmplugin",
        "GMPlugin",
        "PluginContainer",
        "GMPlugin",
        "GmpPlugin",
        "GMPLUGIN",
        True,
    ),
    GeckoProcessType(
        5,
        "GPU",
        "gpu",
        "GPU",
        "Self",
        "GPU",
        "Gpu",
        "GPU",
        True,
    ),
    GeckoProcessType(
        6,
        "VR",
        "vr",
        "VR",
        "Self",
        "VR",
        "Vr",
        "VR",
        True,
    ),
    GeckoProcessType(
        7,
        "RDD",
        "rdd",
        "RDD",
        "Self",
        "RDD",
        "Rdd",
        "RDD",
        True,
    ),
    GeckoProcessType(
        8,
        "Socket",
        "socket",
        "Socket",
        "Self",
        "Socket",
        "Socket",
        "SOCKET",
        True,
    ),
    GeckoProcessType(
        9,
        "RemoteSandboxBroker",
        "sandboxbroker",
        "RemoteSandboxBroker",
        "PluginContainer",
        "RemoteSandboxBroker",
        "RemoteSandboxBroker",
        "REMOTESANDBOXBROKER",
        True,
    ),
    GeckoProcessType(
        10,
        "ForkServer",
        "forkserver",
        "ForkServer",
        "Self",
        "ForkServer",
        "ForkServer",
        "FORKSERVER",
        True,
    ),
    GeckoProcessType(
        11,
        "Utility",
        "utility",
        "Utility",
        "Self",
        "Utility",
        "Utility",
        "UTILITY",
        True,
    ),
]
