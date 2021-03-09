/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class GeckoCreditCardsAddressesStorageDelegateTest {

    private val storage = AutofillCreditCardsAddressesStorage(testContext)
    private lateinit var delegate: GeckoCreditCardsAddressesStorageDelegate
    private lateinit var scope: TestCoroutineScope

    init {
        testContext.getDatabasePath(AUTOFILL_DB_NAME)!!.parentFile!!.mkdirs()
    }

    @Before
    fun before() = runBlocking {
        scope = TestCoroutineScope()
        delegate = GeckoCreditCardsAddressesStorageDelegate(lazy { storage }, scope)
    }

    @Test
    fun `onAddressFetch`() {
        scope.launch {
            delegate.onAddressesFetch()
            verify(storage, times(1)).getAllAddresses()
        }
    }

    @Test
    fun `onCreditCardFetch`() {
        scope.launch {
            delegate.onCreditCardsFetch()
            verify(storage, times(1)).getAllCreditCards()
        }
    }
}
