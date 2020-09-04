# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Page title
about-processes-title = Process Manager

## Column headers

about-processes-column-id = Id
about-processes-column-name = Name
about-processes-column-memory-resident = Memory
about-processes-column-cpu-total = CPU
about-processes-column-threads = Threads

## Individual cells

about-processes-browser-name = { -brand-short-name }

# Thread
# Variables:
#   $name (String) The name assigned to the thread.
about-processes-thread-name = Thread: { $name }

# Process
# Variables:
#   $name (String) The name assigned to the process.
about-processes-process-name = Process: { $name }

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
