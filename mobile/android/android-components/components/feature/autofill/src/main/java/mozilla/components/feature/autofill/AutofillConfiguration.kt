/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill

import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.feature.autofill.lock.AutofillLock
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.feature.autofill.ui.AbstractAutofillUnlockActivity

/**
 * Configuration for the "Autofill" feature.
 *
 * @property storage The [LoginsStorage] used for looking up accounts and passwords to autofill.
 * @property publicSuffixList Global instance of the public suffix list used for matching domains.
 * @property unlockActivity Activity class that implements [AbstractAutofillUnlockActivity].
 * @property applicationName The name of the application that integrates this feature. Used in UI.
 * @property lock Global [AutofillLock] instance used for unlocking the autofill service.
 * @property activityRequestCode The request code used for pending intents that launch an activity
 * on behalf of the autofill service.
 */
data class AutofillConfiguration(
    val storage: LoginsStorage,
    val publicSuffixList: PublicSuffixList,
    val unlockActivity: Class<*>,
    val applicationName: String,
    val lock: AutofillLock = AutofillLock(),
    val activityRequestCode: Int = 1010
)
