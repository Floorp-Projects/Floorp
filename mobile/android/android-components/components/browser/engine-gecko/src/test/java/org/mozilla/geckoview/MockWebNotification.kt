/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview

class MockWebNotification(
    title: String,
    tag: String,
    cookie: String,
    text: String,
    imageUrl: String,
    textDirection: String,
    lang: String,
    requireInteraction: Boolean,
    source: String
) : WebNotification(title, tag, cookie, text, imageUrl, textDirection, lang, requireInteraction, source)
