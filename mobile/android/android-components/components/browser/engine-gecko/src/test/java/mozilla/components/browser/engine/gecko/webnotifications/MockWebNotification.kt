/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.webnotifications

import mozilla.components.support.test.mock
import mozilla.components.test.ReflectionUtils
import org.mozilla.geckoview.WebNotification

fun mockWebNotification(
    tag: String,
    requireInteraction: Boolean,
    vibrate: IntArray = IntArray(0),
    title: String? = null,
    text: String? = null,
    imageUrl: String? = null,
    textDirection: String? = null,
    lang: String? = null,
    source: String? = null,
    silent: Boolean = false,
): WebNotification {
    val webNotification: WebNotification = mock()
    ReflectionUtils.setField(webNotification, "title", title)
    ReflectionUtils.setField(webNotification, "tag", tag)
    ReflectionUtils.setField(webNotification, "text", text)
    ReflectionUtils.setField(webNotification, "imageUrl", imageUrl)
    ReflectionUtils.setField(webNotification, "textDirection", textDirection)
    ReflectionUtils.setField(webNotification, "lang", lang)
    ReflectionUtils.setField(webNotification, "requireInteraction", requireInteraction)
    ReflectionUtils.setField(webNotification, "source", source)
    ReflectionUtils.setField(webNotification, "silent", silent)
    ReflectionUtils.setField(webNotification, "vibrate", vibrate)
    return webNotification
}
