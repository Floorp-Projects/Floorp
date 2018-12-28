# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY
from fluent.migrate import CONCAT
from fluent.migrate import REPLACE
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import PLURALS, REPLACE_IN_TEXT

def migrate(ctx):
    """Bug 1507595 - Migrate about:support messages to use Fluent for localization, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutSupport.ftl",
        "toolkit/toolkit/about/aboutSupport.ftl",
        transforms_from(
"""
page-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.pageTitle")}
crashes-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.crashes.title")}
crashes-id = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.crashes.id")}
crashes-send-date = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.crashes.sendDate")}
crashes-all-reports = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.crashes.allReports")}
crashes-no-config = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.crashes.noConfig")}
extensions-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.extensionsTitle")}
extensions-name = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.extensionName")}
extensions-enabled = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.extensionEnabled")}
extensions-version = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.extensionVersion")}
extensions-id = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.extensionId")}
security-software-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.securitySoftwareTitle")}
security-software-type = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.securitySoftwareType")}
security-software-name = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.securitySoftwareName")}
security-software-antivirus = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.securitySoftwareAntivirus")}
security-software-antispyware = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.securitySoftwareAntiSpyware")}
security-software-firewall = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.securitySoftwareFirewall")}
features-name = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.featureName")}
features-version = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.featureVersion")}
features-id = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.featureId")}
app-basics-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsTitle")}
app-basics-name = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsName")}
app-basics-version = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsVersion")}
app-basics-build-id = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsBuildID")}
app-basics-update-channel = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsUpdateChannel")}
app-basics-update-history = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsUpdateHistory")}
app-basics-show-update-history = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsShowUpdateHistory")}
app-basics-enabled-plugins = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsEnabledPlugins")}
app-basics-build-config = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsBuildConfig")}
app-basics-user-agent = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsUserAgent")}
app-basics-os = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsOS")}
app-basics-memory-use = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsMemoryUse")}
app-basics-performance = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsPerformance")}
app-basics-service-workers = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsServiceWorkers")}
app-basics-profiles = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsProfiles")}
app-basics-multi-process-support = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsMultiProcessSupport")}
app-basics-process-count = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsProcessCount")}
app-basics-enterprise-policies = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.enterprisePolicies")}
app-basics-key-google = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsKeyGoogle")}
app-basics-key-mozilla = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsKeyMozilla")}
app-basics-safe-mode = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.appBasicsSafeMode")}
modified-key-prefs-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.modifiedKeyPrefsTitle")}
modified-prefs-name = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.modifiedPrefsName")}
modified-prefs-value = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.modifiedPrefsValue")}
user-js-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.userJSTitle")}
locked-key-prefs-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.lockedKeyPrefsTitle")}
locked-prefs-name = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.lockedPrefsName")}
locked-prefs-value = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.lockedPrefsValue")}
graphics-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsTitle")}
graphics-features-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsFeaturesTitle")}
graphics-diagnostics-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsDiagnosticsTitle")}
graphics-failure-log-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsFailureLogTitle")}
graphics-gpu1-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsGPU1Title")}
graphics-gpu2-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsGPU2Title")}
graphics-decision-log-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsDecisionLogTitle")}
graphics-crash-guards-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsCrashGuardsTitle")}
graphics-workarounds-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.graphicsWorkaroundsTitle")}
place-database-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.placeDatabaseTitle")}
place-database-integrity = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.placeDatabaseIntegrity")}
place-database-verify-integrity = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.placeDatabaseVerifyIntegrity")}
js-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.jsTitle")}
js-incremental-gc = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.jsIncrementalGC")}
a11y-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.a11yTitle")}
a11y-activated = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.a11yActivated")}
a11y-force-disabled = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.a11yForceDisabled")}
a11y-handler-used = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.a11yHandlerUsed")}
a11y-instantiator = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.a11yInstantiator")}
library-version-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.libraryVersionsTitle")}
copy-text-to-clipboard-label = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.copyTextToClipboard.label")}
copy-raw-data-to-clipboard-label = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.copyRawDataToClipboard.label")}
sandbox-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxTitle")}
sandbox-sys-call-log-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallLogTitle")}
sandbox-sys-call-index = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallIndex")}
sandbox-sys-call-age = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallAge")}
sandbox-sys-call-pid = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallPID")}
sandbox-sys-call-tid = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallTID")}
sandbox-sys-call-proc-type = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallProcType")}
sandbox-sys-call-number = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallNumber")}
sandbox-sys-call-args = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.sandboxSyscallArgs")}
safe-mode-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.safeModeTitle")}
restart-in-safe-mode-label = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.restartInSafeMode.label")}
media-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaTitle")}
media-output-devices-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaOutputDevicesTitle")}
media-input-devices-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaInputDevicesTitle")}
media-device-name = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceName")}
media-device-group = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceGroup")}
media-device-vendor = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceVendor")}
media-device-state = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceState")}
media-device-preferred = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDevicePreferred")}
media-device-format = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceFormat")}
media-device-channels = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceChannels")}
media-device-rate = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceRate")}
media-device-latency = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.mediaDeviceLatency")}
intl-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlTitle")}
intl-app-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlAppTitle")}
intl-locales-requested = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlLocalesRequested")}
intl-locales-available = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlLocalesAvailable")}
intl-locales-supported = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlLocalesSupported")}
intl-locales-default = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlLocalesDefault")}
intl-os-title = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlOSTitle")}
intl-os-prefs-system-locales = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlOSPrefsSystemLocales")}
intl-regional-prefs = { COPY("toolkit/chrome/global/aboutSupport.dtd", "aboutSupport.intlRegionalPrefs")}
raw-data-copied = { COPY("toolkit/chrome/global/aboutSupport.properties", "rawDataCopied")}
text-copied = { COPY("toolkit/chrome/global/aboutSupport.properties", "textCopied")}
clear-type-parameters = { COPY("toolkit/chrome/global/aboutSupport.properties", "clearTypeParameters")}
compositing = { COPY("toolkit/chrome/global/aboutSupport.properties", "compositing")}
hardware-h264 = { COPY("toolkit/chrome/global/aboutSupport.properties", "hardwareH264")}
main-thread-no-omtc = { COPY("toolkit/chrome/global/aboutSupport.properties", "mainThreadNoOMTC")}
yes = { COPY("toolkit/chrome/global/aboutSupport.properties", "yes")}
no = { COPY("toolkit/chrome/global/aboutSupport.properties", "no")}
found = { COPY("toolkit/chrome/global/aboutSupport.properties", "found")}
missing = { COPY("toolkit/chrome/global/aboutSupport.properties", "missing")}
gpu-description = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuDescription")}
gpu-vendor-id = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuVendorID")}
gpu-device-id = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuDeviceID")}
gpu-subsys-id = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuSubsysID")}
gpu-drivers = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuDrivers")}
gpu-ram = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuRAM")}
gpu-driver-version = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuDriverVersion")}
gpu-driver-date = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuDriverDate")}
gpu-active = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuActive")}
webgl1-wsiinfo = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl1WSIInfo")}
webgl1-renderer = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl1Renderer")}
webgl1-version = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl1Version")}
webgl1-driver-extensions = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl1DriverExtensions")}
webgl1-extensions = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl1Extensions")}
webgl2-wsiinfo = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl2WSIInfo")}
webgl2-renderer = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl2Renderer")}
webgl2-version = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl2Version")}
webgl2-driver-extensions = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl2DriverExtensions")}
webgl2-extensions = { COPY("toolkit/chrome/global/aboutSupport.properties", "webgl2Extensions")}
blocklisted-bug = { COPY("toolkit/chrome/global/aboutSupport.properties", "blocklistedBug")}
d3d11layers-crash-guard = { COPY("toolkit/chrome/global/aboutSupport.properties", "d3d11layersCrashGuard")}
d3d11video-crash-guard = { COPY("toolkit/chrome/global/aboutSupport.properties", "d3d11videoCrashGuard")}
d3d9video-crash-buard = { COPY("toolkit/chrome/global/aboutSupport.properties", "d3d9videoCrashGuard")}
glcontext-crash-guard = { COPY("toolkit/chrome/global/aboutSupport.properties", "glcontextCrashGuard")}
reset-on-next-restart = { COPY("toolkit/chrome/global/aboutSupport.properties", "resetOnNextRestart")}
gpu-process-kill-button = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuProcessKillButton")}
gpu-device-reset-button = { COPY("toolkit/chrome/global/aboutSupport.properties", "gpuDeviceResetButton")}
uses-tiling = { COPY("toolkit/chrome/global/aboutSupport.properties", "usesTiling")}
content-uses-tiling = { COPY("toolkit/chrome/global/aboutSupport.properties", "contentUsesTiling")}
off-main-thread-paint-enabled = { COPY("toolkit/chrome/global/aboutSupport.properties", "offMainThreadPaintEnabled")}
off-main-thread-paint-worker-count = { COPY("toolkit/chrome/global/aboutSupport.properties", "offMainThreadPaintWorkerCount")}
audio-backend = { COPY("toolkit/chrome/global/aboutSupport.properties", "audioBackend")}
max-audio-channels = { COPY("toolkit/chrome/global/aboutSupport.properties", "maxAudioChannels")}
channel-layout = { COPY("toolkit/chrome/global/aboutSupport.properties", "channelLayout")}
sample-rate = { COPY("toolkit/chrome/global/aboutSupport.properties", "sampleRate")}
min-lib-versions = { COPY("toolkit/chrome/global/aboutSupport.properties", "minLibVersions")}
loaded-lib-versions = { COPY("toolkit/chrome/global/aboutSupport.properties", "loadedLibVersions")}
has-seccomp-bpf = { COPY("toolkit/chrome/global/aboutSupport.properties", "hasSeccompBPF")}
has-seccomp-tsync = { COPY("toolkit/chrome/global/aboutSupport.properties", "hasSeccompTSync")}
has-user-namespaces = { COPY("toolkit/chrome/global/aboutSupport.properties", "hasUserNamespaces")}
has-privileged-user-namespaces = { COPY("toolkit/chrome/global/aboutSupport.properties", "hasPrivilegedUserNamespaces")}
can-sandbox-content = { COPY("toolkit/chrome/global/aboutSupport.properties", "canSandboxContent")}
can-sandbox-media = { COPY("toolkit/chrome/global/aboutSupport.properties", "canSandboxMedia")}
content-sandbox-level = { COPY("toolkit/chrome/global/aboutSupport.properties", "contentSandboxLevel")}
effective-content-sandbox-level = { COPY("toolkit/chrome/global/aboutSupport.properties", "effectiveContentSandboxLevel")}
sandbox-proc-type-content = { COPY("toolkit/chrome/global/aboutSupport.properties", "sandboxProcType.content")}
sandbox-proc-type-file = { COPY("toolkit/chrome/global/aboutSupport.properties", "sandboxProcType.file")}
sandbox-proc-type-media-plugin = { COPY("toolkit/chrome/global/aboutSupport.properties", "sandboxProcType.mediaPlugin")}
multi-process-status-0 = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.0")}
multi-process-status-1 = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.1")}
multi-process-status-2 = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.2")}
multi-process-status-4 = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.4")}
multi-process-status-6 = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.6")}
multi-process-status-7 = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.7")}
multi-process-status-8 = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.8")}
multi-process-status-unknown = { COPY("toolkit/chrome/global/aboutSupport.properties", "multiProcessStatus.unknown")}
async-pan-zoom = { COPY("toolkit/chrome/global/aboutSupport.properties", "asyncPanZoom")}
apz-none = { COPY("toolkit/chrome/global/aboutSupport.properties", "apzNone")}
wheel-enabled = { COPY("toolkit/chrome/global/aboutSupport.properties", "wheelEnabled")}
touch-enabled = { COPY("toolkit/chrome/global/aboutSupport.properties", "touchEnabled")}
drag-enabled = { COPY("toolkit/chrome/global/aboutSupport.properties", "dragEnabled")}
keyboard-enabled = { COPY("toolkit/chrome/global/aboutSupport.properties", "keyboardEnabled")}
autoscroll-enabled = { COPY("toolkit/chrome/global/aboutSupport.properties", "autoscrollEnabled")}
policies-inactive = { COPY("toolkit/chrome/global/aboutSupport.properties", "policies.inactive")}
policies-active = { COPY("toolkit/chrome/global/aboutSupport.properties", "policies.active")}
policies-error = { COPY("toolkit/chrome/global/aboutSupport.properties", "policies.error")}
multi-process-windows = { $remoteWindows }/{ $totalWindows }
""")
)

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutSupport.ftl",
        "toolkit/toolkit/about/aboutSupport.ftl",
        [
        FTL.Message(
            id=FTL.Identifier("page-subtitle"),
            value=REPLACE(
                "toolkit/chrome/global/aboutSupport.dtd",
                "aboutSupport.pageSubtitle",
                {
                    "&brandShortName;": TERM_REFERENCE("-brand-short-name"),
                    "<a id='supportLink'>": FTL.TextElement('<a data-l10n-name="support-link">'),
                },
                trim=True
            )
        ),
        FTL.Message(
            id=FTL.Identifier("features-title"),
            value=REPLACE(
                "toolkit/chrome/global/aboutSupport.dtd",
                "aboutSupport.featuresTitle",
                {
                    "&brandShortName;": TERM_REFERENCE("-brand-short-name"),
                },
            )
        ),
        FTL.Message(
            id=FTL.Identifier("app-basics-profile-dir"),
            value=FTL.Pattern(
                elements=[
                    FTL.Placeable(
                        expression=FTL.SelectExpression(
                            selector=FTL.CallExpression(
                                callee=FTL.Function("PLATFORM")
                            ),
                            variants=[
                                FTL.Variant(
                                    key=FTL.Identifier("linux"),
                                    default=False,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutSupport.dtd",
                                        "aboutSupport.appBasicsProfileDir"
                                    )
                                ),
                                FTL.Variant(
                                    key=FTL.Identifier("other"),
                                    default=True,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutSupport.dtd",
                                        "aboutSupport.appBasicsProfileDirWinMac"
                                    )
                                ),
                            ]
                        )
                    )
                ]
            )
        ),
        FTL.Message(
            id=FTL.Identifier("show-dir-label"),
            value=FTL.Pattern(
                elements=[
                    FTL.Placeable(
                        expression=FTL.SelectExpression(
                            selector=FTL.CallExpression(
                                callee=FTL.Function("PLATFORM")
                            ),
                            variants=[
                                FTL.Variant(
                                    key=FTL.Identifier("macos"),
                                    default=False,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutSupport.dtd",
                                        "aboutSupport.showMac.label"
                                    )
                                ),
                                FTL.Variant(
                                    key=FTL.Identifier("windows"),
                                    default=False,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutSupport.dtd",
                                        "aboutSupport.showWin2.label"
                                    )
                                ),
                                FTL.Variant(
                                    key=FTL.Identifier("other"),
                                    default=True,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutSupport.dtd",
                                        "aboutSupport.showDir.label"
                                    )
                                ),
                            ]
                        )
                    )
                ]
            )
        ),
        FTL.Message(
            id=FTL.Identifier("user-js-description"),
            value=REPLACE(
                "toolkit/chrome/global/aboutSupport.dtd",
                "aboutSupport.userJSDescription",
                {
                    "<a id='prefs-user-js-link'>": FTL.TextElement('<a data-l10n-name="user-js-link">'),
                    "&brandShortName;": TERM_REFERENCE("-brand-short-name"),
                },
            )
        ),
        FTL.Message(
            id=FTL.Identifier("report-crash-for-days"),
            value=PLURALS(
                "toolkit/chrome/global/aboutSupport.properties",
                "crashesTitle",
                VARIABLE_REFERENCE("days"),
                lambda text: REPLACE_IN_TEXT(
                    text,
                    {
                        "#1": VARIABLE_REFERENCE("days")
                    }
                )
            )
        ),
        FTL.Message(
            id=FTL.Identifier("crashes-time-minutes"),
            value=PLURALS(
                "toolkit/chrome/global/aboutSupport.properties",
                "crashesTimeMinutes",
                VARIABLE_REFERENCE("minutes"),
                lambda text: REPLACE_IN_TEXT(
                    text,
                    {
                        "#1": VARIABLE_REFERENCE("minutes")
                    }
                )
            )
        ),
        FTL.Message(
            id=FTL.Identifier("crashes-time-hours"),
            value=PLURALS(
                "toolkit/chrome/global/aboutSupport.properties",
                "crashesTimeHours",
                VARIABLE_REFERENCE("hours"),
                lambda text: REPLACE_IN_TEXT(
                    text,
                    {
                        "#1": VARIABLE_REFERENCE("hours")
                    }
                )
            )
        ),
        FTL.Message(
            id=FTL.Identifier("crashes-time-days"),
            value=PLURALS(
                "toolkit/chrome/global/aboutSupport.properties",
                "crashesTimeDays",
                VARIABLE_REFERENCE("days"),
                lambda text: REPLACE_IN_TEXT(
                    text,
                    {
                        "#1": VARIABLE_REFERENCE("days")
                    }
                )
            )
        ),
        FTL.Message(
            id=FTL.Identifier("pending-reports"),
            value=PLURALS(
                "toolkit/chrome/global/aboutSupport.properties",
                "pendingReports",
                VARIABLE_REFERENCE("reports"),
                lambda text: REPLACE_IN_TEXT(
                    text,
                    {
                        "#1": VARIABLE_REFERENCE("reports")
                    }
                )
            )
        ),
        FTL.Message(
            id=FTL.Identifier("bug-link"),
            value=REPLACE(
                "toolkit/chrome/global/aboutSupport.properties",
                "bugLink",
                {
                    "%1$S": VARIABLE_REFERENCE("bugNumber"),
                },
            )
        ),
        FTL.Message(
            id=FTL.Identifier("unknown-failure"),
            value=REPLACE(
                "toolkit/chrome/global/aboutSupport.properties",
                "unknownFailure",
                {
                    "%1$S": VARIABLE_REFERENCE("failureCode"),
                },
            )
        ),
        FTL.Message(
            id=FTL.Identifier("wheel-warning"),
            value=REPLACE(
                "toolkit/chrome/global/aboutSupport.properties",
                "wheelWarning",
                {
                    "%S": VARIABLE_REFERENCE("preferenceKey"),
                },
            )
        ),
        FTL.Message(
            id=FTL.Identifier("touch-warning"),
            value=REPLACE(
                "toolkit/chrome/global/aboutSupport.properties",
                "touchWarning",
                {
                    "%S": VARIABLE_REFERENCE("preferenceKey"),
                },
            )
        ),
   ]
)
