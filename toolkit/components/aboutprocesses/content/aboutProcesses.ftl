# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Page title
about-processes-title = Process Manager

## Column headers

about-processes-column-name = Name
about-processes-column-memory-resident = Memory
about-processes-column-cpu-total = CPU

## Process names
## Variables:
##    $pid (String) The process id of this process, assigned by the OS.
##    $origin (String) The domain name for this process.
##    $type (String) The raw type for this process. Used for unknown processes.

about-processes-browser-process-name = { -brand-short-name } (process { $pid })
about-processes-web-process-name = Web (process { $pid }, shared)
about-processes-webIsolated-process-name = Web (process { $pid }) for { $origin }
about-processes-webLargeAllocation = Web (process { $pid }, large) for { $origin }
about-processes-withCoopCoep-process-name = Web (process { $pid }, cross-origin isolated) for { $origin }
about-processes-file-process-name = Files (process { $pid })
about-processes-extension-process-name = Extensions (process { $pid })
about-processes-privilegedabout-process-name = About (process { $pid })
about-processes-plugin-process-name = Plugins (process { $pid })
about-processes-privilegedmozilla-process-name = Web (process { $pid }) for { -vendor-short-name } sites
about-processes-gmpPlugin-process-name = Gecko Media Plugins (process { $pid })
about-processes-gpu-process-name = GPU (process { $pid })
about-processes-vr-process-name = VR (process { $pid })
about-processes-rdd-process-name = Data Decoder (process { $pid })
about-processes-socket-process-name = Network (process { $pid })
about-processes-remoteSandboxBroker-process-name = Remote Sandbox Broker (process { $pid })
about-processes-forkServer-process-name = Fork Server (process { $pid })
about-processes-preallocated-process-name = Preallocated (process { $pid })
about-processes-unknown-process-name = Other ({$ type }, process { $pid })


# Process
# Variables:
#   $name (String) The name assigned to the process.
#   $pid (String) The process id of this process, assigned by the OS.
about-processes-process-name = Process { $pid }: { $name }

## Details within processes

# Single-line summary of threads
# Variables:
#    $number (Number) The number of threads in the process. Typically larger
#                     than 30. We don't expect to ever have processes with less
#                     than 5 threads.
about-processes-thread-summary = Threads ({ $number })

# Thread details
# Variables:
#   $name (String) The name assigned to the thread.
#   $tid (String) The thread id of this thread, assigned by the OS.
about-processes-thread-name = Thread { $tid }: { $name }


# Extension
# Variables:
#   $name (String) The name of the extension.
about-processes-extension-name = Extension: { $name }

# Tab
# Variables:
#   $name (String) The name of the tab (typically the title of the page, might be the url while the page is loading).
about-processes-tab-name = Tab: { $name }
about-processes-preloaded-tab = Preloaded New Tab

# Single subframe
# Variables:
#   $url (String) The full url of this subframe.
about-processes-frame-name-one = Subframe: { $url }

# Group of subframes
# Variables:
#   $number (Number) The number of subframes in this group. Always ≥ 1.
#   $shortUrl (String) The shared prefix for the subframes in the group.
about-processes-frame-name-many = Subframes ({ $number }): { $shortUrl }
