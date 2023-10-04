# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This is the title of the page
about-logging-title = About Logging
about-logging-page-title = Logging manager
about-logging-current-log-file = Current log file:
about-logging-new-log-file = New log file:
about-logging-currently-enabled-log-modules = Currently enabled log modules:
about-logging-log-tutorial =
    See <a data-l10n-name="logging">HTTP Logging</a>
    for instructions on how to use this tool.
# This message is used as a button label, "Open" indicates an action.
about-logging-open-log-file-dir = Open directory
about-logging-set-log-file = Set Log File
about-logging-set-log-modules = Set Log Modules
about-logging-start-logging = Start Logging
about-logging-stop-logging = Stop Logging
about-logging-buttons-disabled = Logging configured via environment variables, dynamic configuration unavailable.
about-logging-some-elements-disabled = Logging configured via URL, some configuration options are unavailable
about-logging-info = Info:
about-logging-log-modules-selection = Log module selection
about-logging-new-log-modules = New log modules:
about-logging-logging-output-selection = Logging output
about-logging-logging-to-file = Logging to a file
about-logging-logging-to-profiler = Logging to the { -profiler-brand-name }
about-logging-no-log-modules = None
about-logging-no-log-file = None
about-logging-logging-preset-selector-text = Logging preset:
about-logging-with-profiler-stacks-checkbox = Enable stack traces for log messages

## Logging presets

about-logging-preset-networking-label = Networking
about-logging-preset-networking-description = Log modules to diagnose networking issues
about-logging-preset-networking-cookie-label = Cookies
about-logging-preset-networking-cookie-description = Log modules to diagnose cookie issues
about-logging-preset-networking-websocket-label = WebSockets
about-logging-preset-networking-websocket-description = Log modules to diagnose WebSocket issues
about-logging-preset-networking-http3-label = HTTP/3
about-logging-preset-networking-http3-description = Log modules to diagnose HTTP/3 and QUIC issues
about-logging-preset-media-playback-label = Media playback
about-logging-preset-media-playback-description = Log modules to diagnose media playback issues (not video-conferencing issues)
about-logging-preset-webrtc-label = WebRTC
about-logging-preset-webrtc-description = Log modules to diagnose WebRTC calls
about-logging-preset-webgpu-label = WebGPU
about-logging-preset-webgpu-description = Log modules to diagnose WebGPU issues
about-logging-preset-gfx-label = Graphics
about-logging-preset-gfx-description = Log modules to diagnose graphics issues
# This is specifically "Microsoft Windows". Microsoft normally doesn't localize it, and we should follow their convention here.
about-logging-preset-windows-label = Windows
about-logging-preset-windows-description = Log modules to diagnose issues specific to Microsoft Windows
about-logging-preset-custom-label = Custom
about-logging-preset-custom-description = Log modules manually selected

# Error handling
about-logging-error = Error:

## Variables:
##   $k (String) - Variable name
##   $v (String) - Variable value

about-logging-invalid-output = Invalid value “{ $v }“ for key “{ $k }“
about-logging-unknown-logging-preset = Unknown logging preset “{ $v }“
about-logging-unknown-profiler-preset = Unknown profiler preset “{ $v }“
about-logging-unknown-option = Unknown about:logging option “{ $k }“
about-logging-configuration-url-ignored = Configuration URL ignored
about-logging-file-and-profiler-override = Can’t force file output and override profiler options at the same time

about-logging-configured-via-url = Option configured via URL
