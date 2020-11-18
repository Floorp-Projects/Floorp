/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.accounts.push.ext

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class StringKtTest {

    @Test
    fun `redacted endpoint contains short form in it`() {
        val endpoint =
            "https://updates.push.services.mozilla.com/wpush/v1/gAAAAABfAL4OBMRjP3O66lugUrcHT8kk4ENnJP4SE67US" +
                "kmH9NdIz__-_3PtC_V79-KwG73Y3mZye1qtnYzoJETaGQidjgbiJdXzB7u0T9BViE2b7O3oqsFJpnwvmO-CiFqKKP14vitH"

        assertTrue(endpoint.redactPartialUri().contains("redacted..."))
    }
}