#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This script is run by the update smoketest frontend

ADB=${ADB:-adb}
DEVICE=$1

run_adb() {
    $ADB -s $DEVICE $@
}

run_adb push %(flash_zip)s %(sdcard)s/_flash.zip
run_adb shell 'echo -n "--update_package=%(sdcard_recovery)s/_flash.zip" > /cache/recovery/command'
run_adb reboot recovery
