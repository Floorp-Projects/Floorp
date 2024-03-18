/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.activity

import android.app.PendingIntent
import android.content.Intent
import mozilla.components.concept.engine.activity.ActivityDelegate
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoRuntime
import java.lang.ref.WeakReference

/**
 * A wrapper for the [ActivityDelegate] to communicate with the Gecko-based delegate.
 */
internal class GeckoActivityDelegate(
    private val delegateRef: WeakReference<ActivityDelegate>,
) : GeckoRuntime.ActivityDelegate {

    private val logger = Logger(GeckoActivityDelegate::javaClass.name)

    override fun onStartActivityForResult(intent: PendingIntent): GeckoResult<Intent> {
        val result: GeckoResult<Intent> = GeckoResult()
        val delegate = delegateRef.get()

        if (delegate == null) {
            logger.warn("No activity delegate attached. Cannot request FIDO auth.")

            result.completeExceptionally(RuntimeException("Activity for result failed; no delegate attached."))

            return result
        }

        delegate.startIntentSenderForResult(intent.intentSender) { data ->
            if (data != null) {
                result.complete(data)
            } else {
                result.completeExceptionally(RuntimeException("Activity for result failed."))
            }
        }
        return result
    }
}
