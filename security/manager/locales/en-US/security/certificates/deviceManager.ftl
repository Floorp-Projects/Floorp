# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## Strings used for device manager

devmgr-window =
    .title = Device Manager
    .style = min-width: 67em; min-height: 32em;

devmgr-devlist =
    .label = Security Modules and Devices

devmgr-header-details =
    .label = Details

devmgr-header-value =
    .label = Value

devmgr-button-login =
    .label = Log In
    .accesskey = n

devmgr-button-logout =
    .label = Log Out
    .accesskey = O

devmgr-button-changepw =
    .label = Change Password
    .accesskey = P

devmgr-button-load =
    .label = Load
    .accesskey = L

devmgr-button-unload =
    .label = Unload
    .accesskey = U

devmgr-button-enable-fips =
    .label = Enable FIPS
    .accesskey = F

devmgr-button-disable-fips =
    .label = Disable FIPS
    .accesskey = F

## Strings used for load device

load-device =
    .title = Load PKCS#11 Device Driver

load-device-info = Enter the information for the module you want to add.

load-device-modname =
    .value = Module Name
    .accesskey = M

load-device-modname-default =
    .value = New PKCS#11 Module

load-device-filename =
    .value = Module filename
    .accesskey = f

load-device-browse =
    .label = Browse…
    .accesskey = B

## Token Manager

devinfo-status =
    .label = Status

devinfo-status-disabled =
    .label = Disabled

devinfo-status-not-present =
    .label = Not Present

devinfo-status-uninitialized =
    .label = Uninitialized

devinfo-status-not-logged-in =
    .label = Not Logged In

devinfo-status-logged-in =
    .label = Logged In

devinfo-status-ready =
    .label = Ready

devinfo-desc =
    .label = Description

devinfo-man-id =
    .label = Manufacturer

devinfo-hwversion =
    .label = HW Version
devinfo-fwversion =
    .label = FW Version

devinfo-modname =
    .label = Module

devinfo-modpath =
    .label = Path

login-failed = Failed to Login

devinfo-label =
    .label = Label

devinfo-serialnum =
    .label = Serial Number

fips-nonempty-primary-password-required = FIPS mode requires that you have a Primary Password set for each security device. Please set the password before trying to enable FIPS mode.
unable-to-toggle-fips = Unable to change the FIPS mode for the security device. It is recommended that you exit and restart this application.
load-pk11-module-file-picker-title = Choose a PKCS#11 device driver to load

# Load Module Dialog
load-module-help-empty-module-name =
    .value = The module name cannot be empty.

# Do not translate 'Root Certs'
load-module-help-root-certs-module-name =
    .value = ‘Root Certs‘ is reserved and cannot be used as the module name.

add-module-failure = Unable to add module
del-module-warning = Are you sure you want to delete this security module?
del-module-error = Unable to delete module
