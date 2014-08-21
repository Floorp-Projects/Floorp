/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <assert.h>
#include "webrtc/modules/desktop_capture/win/win_shared.h"

namespace webrtc {

std::string Utf16ToUtf8(const WCHAR* str) {
    int len_utf8 = WideCharToMultiByte(CP_UTF8, 0, str, -1,
                                       NULL, 0, NULL, NULL);
    if (len_utf8 <= 0) {
        return std::string();
    }
    std::string result(len_utf8, '\0');
    int rv = WideCharToMultiByte(CP_UTF8, 0, str, -1,
                                 &*(result.begin()), len_utf8,
                                 NULL, NULL);
    if (rv != len_utf8) {
        assert(false);
    }

    return result;
}

};
