# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

history-title = Update History
history-intro = The following updates have been installed

close-button-label =
    .buttonlabelcancel = Close
    .title = Update History

no-updates-label = No updates installed yet
name-header = Update Name
date-header = Install Date
type-header = Type
state-header = State

# Used to display update history
#
# Variables:
#   $name (string) - Name of the update
#   $buildID (string) - Build identifier from the local updates.xml
update-full-build-name = { $name } ({ $buildID })

update-details = Details

# Variables:
#   $date (string) - Date the last update was installed
update-installed-on = Installed on: { $date }

# Variables:
#   $status (string) - Status of the last update
update-status = Status: { $status }
