/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview

import org.robolectric.util.ReflectionHelpers.setField

class MockWebResponseInfo(
    uri: String,
    contentType: String,
    contentLength: Long,
    filename: String?
) : GeckoSession.WebResponseInfo() {
    init {
        setField(this, "uri", uri)
        setField(this, "contentType", contentType)
        setField(this, "filename", filename)
        setField(this, "contentLength", contentLength)
    }
}
