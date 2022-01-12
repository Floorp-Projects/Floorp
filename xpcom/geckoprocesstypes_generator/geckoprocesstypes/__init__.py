# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import namedtuple

# Besides adding a new process type to this file, a bit of extra work is
# required for places where generated code has not yet been implemented.
# TODO: Bug 1740268: Move this piece into doc and link doc from here.
#
# Basic requirements:
#  dom/chrome-webidl/ChromeUtils.webidl
#   - Add a new entry to the enum WebIDLProcType
#
#  gfx/thebes/gfxPlatform.cpp
#   - (if you need GFX related stuff ?)
#   - Add a call to your process manager init in gfxPlatform::Init()
#   - Add a call to your process manager shutdown in gfxPlatform::Shutdown()
#
#  mobile/android/geckoview/src/main/AndroidManifest.xml
#   - Add a new <service> entry targetting
#     org.mozilla.gecko.process.GeckoChildProcessServices$XXX
#
#  mobile/android/geckoview/src/main/java/org/mozilla/gecko/process/GeckoChildProcessServices.jinja
#   - Add matching class inheritance from GeckoChildProcessServices
#
#  mobile/android/geckoview/src/main/java/org/mozilla/gecko/process/GeckoProcessType.java
#   - Add new entry in public enum GeckoProcessType
#
#  toolkit/crashreporter/CrashAnnotations.yaml
#   - Add new Xxx*Status entry for your new process type description
#
#  toolkit/components/telemetry/Histograms.json
#   - Add entry in PROCESS_CRASH_SUBMIT_ATTEMPT
#
#  toolkit/locales/en-US/toolkit/global/processTypes.ftl
#   - Add a user-facing localizable name for your process, if needed
#
#  toolkit/modules/ProcessType.jsm
#   - Hashmap from process type to user-facing string above in const ProcessType
#
#  toolkit/xre/nsAppRunner.cpp
#   - Update the static_assert call checking for boundary against
#     GeckoProcessType_End
#
#  toolkit/xre/nsEmbedFunctions.cpp
#   - Add your process to the correct MessageLoop::TYPE_x in the first
#     switch(XRE_GetProcessType()) in XRE_InitChildProcess
#   - Instantiate your child within the second switch (XRE_GetProcessType())
#     in XRE_InitChildProcess
#
#  xpcom/system/nsIXULRuntime.idl
#   - Add a new entry PROCESS_TYPE_x in nsIXULRuntime interface
#
#
# For static components:
#  modules/libpref/components.conf
#  toolkit/components/telemetry/core/components.conf
#  widget/android/components.conf
#  widget/gtk/components.conf
#  widget/windows/components.conf
#  xpcom/base/components.conf
#  xpcom/build/components.conf
#  xpcom/components/components.conf
#  xpcom/ds/components.conf
#  xpcom/threads/components.conf
#  widget/cocoa/nsWidgetFactory.mm
#  xpcom/build/XPCOMInit.cpp
#   - Update allowance in those config file to match new process selector
#     including your new process
#
#  xpcom/components/gen_static_components.py
#   - Add new definition in ProcessSelector for your new process
#     ALLOW_IN_x_PROCESS = 0x..
#   - Add new process selector masks including your new process definition
#   - Also add those into the PROCESSES structure
#
#  xpcom/components/Module.h
#   - Add new definition in enum ProcessSelector
#   - Add new process selector mask including the new definition
#   - Update kMaxProcessSelector
#
#  xpcom/components/nsComponentManager.cpp
#   - Add new selector match in ProcessSelectorMatches for your new process
#     (needed?)
#   - Add new process selector for gProcessMatchTable
#     in nsComponentManagerImpl::Init()
#  xpcom/build/XPCOMInit.cpp
#
#
# For sandbox:
#  Sandbox Linux:
#   security/sandbox/linux/Sandbox.cpp
#    - Add new SetXXXSandbox() function
#   security/sandbox/linux/SandboxFilter.cpp
#    - Add new helper GetXXXSandboxPolicy() called by SetXXXSandbox()
#    - Derive new class inheriting SandboxPolicyCommon or SandboxPolicyBase and
#      defining the sandboxing policy
#   security/sandbox/linux/broker/SandboxBrokerPolicyFactory.cpp
#    - Add new SandboxBrokerPolicyFactory::GetXXXProcessPolicy()
#   security/sandbox/linux/launch/SandboxLaunch.cpp
#    - Add new case handling in GetEffectiveSandboxLevel()
#   security/sandbox/linux/reporter/SandboxReporter.cpp
#    - Add new case handling in SubmitToTelemetry()
#   security/sandbox/linux/reporter/SandboxReporterCommon.h
#    - Add new entry in enum class ProcType
#   security/sandbox/linux/reporter/SandboxReporterWrappers.cpp
#    - Add new case handling in SandboxReportWrapper::GetProcType()
#
#  Sandbox Mac:
#   ipc/glue/GeckoChildProcessHost.cpp
#    - Add new case handling in GeckoChildProcessHost::StartMacSandbox()
#   security/sandbox/mac/Sandbox.h
#    - Add new entry in enum MacSandboxType
#   security/sandbox/mac/Sandbox.mm
#    - Handle the new MacSandboxType in MacSandboxInfo::AppendAsParams()
#    - Handle the new MacSandboxType in StartMacSandbox()
#    - Handle the new MacSandboxType in StartMacSandboxIfEnabled()
#   security/sandbox/mac/SandboxPolicy<XXX>.h
#    - Create this new file for your new process <XXX>, it defines the new
#      sandbox
#   security/sandbox/mac/moz.build
#    - Add the previous new file
#
#  Sandbox Win:
#   ipc/glue/GeckoChildProcessHost.cpp
#    - Add new case handling in WindowsProcessLauncher::DoSetup() calling
#      SandboxBroker::SetSecurityLevelForXXXProcess()
#   security/sandbox/win/src/remotesandboxbroker/remoteSandboxBroker.cpp
#    - Introduce a new SandboxBroker::SetSecurityLevelForXXXProcess()
#   security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp
#    - Introduce a new SandboxBroker::SetSecurityLevelForXXXProcess() that
#      defines the new sandbox
#
#  Sandbox tests:
#   - Your new process needs to implement PSandboxTesting.ipdl
#   security/sandbox/test/browser_sandbox_test.js
#    - Add your new process string_name in the processTypes list
#   security/sandbox/common/test/SandboxTest.cpp
#    - Add a new case in SandboxTest::StartTests() to handle your new process
#   security/sandbox/common/test/SandboxTestingChild.cpp
#    - Add a new if branch for your new process in SandboxTestingChild::Bind()
#   security/sandbox/common/test/SandboxTestingChildTests.h
#    - Add a new RunTestsXXX function for your new process (called by Bind() above)
#
# ~~~~~
#
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
        False,
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
        "PluginContainer",
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
        False,
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
        False,
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
        False,
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
