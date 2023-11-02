/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shopping.middleware

import android.content.Context
import android.net.Uri
import org.mozilla.fenix.settings.SupportUtils

private const val PARAM_UTM_CAMPAIGN_KEY = "utm_campaign"
private const val PARAM_UTM_CAMPAIGN_VALUE = "fakespot-by-mozilla"
private const val PARAM_UTM_TERM_KEY = "utm_term"
private const val PARAM_UTM_TERM_VALUE = "core-sheet"

/**
 * Class used to retrieve the SUMO review quality check link.
 *
 * @param context Context used to localize the SUMO url.
 */
class GetReviewQualityCheckSumoUrl(
    private val context: Context,
) {
    private val url by lazy {
        appendUTMParams(
            SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.REVIEW_QUALITY_CHECK),
        )
    }

    /**
     * Returns the review quality check SUMO url.
     */
    operator fun invoke(): String = url

    private fun appendUTMParams(url: String): String = Uri.parse(url).buildUpon()
        .appendQueryParameter(PARAM_UTM_CAMPAIGN_KEY, PARAM_UTM_CAMPAIGN_VALUE)
        .appendQueryParameter(PARAM_UTM_TERM_KEY, PARAM_UTM_TERM_VALUE)
        .build().toString()
}
