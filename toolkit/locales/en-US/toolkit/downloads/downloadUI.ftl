# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

download-ui-confirm-title = Cancel All Downloads?

## Variables:
##   $downloadsCount (Number): The current downloads count.

download-ui-confirm-quit-cancel-downloads =
    { $downloadsCount ->
        [1] If you exit now, 1 download will be canceled. Are you sure you want to exit?
       *[other] If you exit now, { $downloadsCount } downloads will be canceled. Are you sure you want to exit?
    }
download-ui-confirm-quit-cancel-downloads-mac =
    { $downloadsCount ->
        [1] If you quit now, 1 download will be canceled. Are you sure you want to quit?
       *[other] If you quit now, { $downloadsCount } downloads will be canceled. Are you sure you want to quit?
    }
download-ui-dont-quit-button =
    { PLATFORM() ->
        [mac] Don’t Quit
       *[other] Don’t Exit
    }

download-ui-confirm-offline-cancel-downloads =
    { $downloadsCount ->
        [1] If you go offline now, 1 download will be canceled. Are you sure you want to go offline?
       *[other] If you go offline now, { $downloadsCount } downloads will be canceled. Are you sure you want to go offline?
    }
download-ui-dont-go-offline-button = Stay Online

download-ui-confirm-leave-private-browsing-windows-cancel-downloads =
    { $downloadsCount ->
        [1] If you close all Private Browsing windows now, 1 download will be canceled. Are you sure you want to leave Private Browsing?
       *[other] If you close all Private Browsing windows now, { $downloadsCount } downloads will be canceled. Are you sure you want to leave Private Browsing?
    }
download-ui-dont-leave-private-browsing-button = Stay in Private Browsing

download-ui-cancel-downloads-ok =
    { $downloadsCount ->
        [1] Cancel 1 Download
       *[other] Cancel { $downloadsCount } Downloads
    }

##

download-ui-file-executable-security-warning-title = Open Executable File?
# Variables:
#   $executable (String): The executable file to be opened.
download-ui-file-executable-security-warning = “{ $executable }” is an executable file. Executable files may contain viruses or other malicious code that could harm your computer. Use caution when opening this file. Are you sure you want to launch “{ $executable }”?
