/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview

import mozilla.components.test.ReflectionUtils

class MockWebResponseInfo(
    uri: String,
    contentType: String,
    contentLength: Long,
    filename: String?
) : GeckoSession.WebResponseInfo() {
    init {
        ReflectionUtils.setField(this, "uri", uri)
        ReflectionUtils.setField(this, "contentType", contentType)
        ReflectionUtils.setField(this, "filename", filename)
        ReflectionUtils.setField(this, "contentLength", contentLength)
    }
}
