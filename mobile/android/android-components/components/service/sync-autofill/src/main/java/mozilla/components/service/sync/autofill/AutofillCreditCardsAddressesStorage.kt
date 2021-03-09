/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.autofill

import android.content.Context
import androidx.annotation.GuardedBy
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.withContext
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.CreditCardsAddressesStorage
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import java.io.Closeable
import mozilla.appservices.autofill.Store as RustAutofillStorage

const val AUTOFILL_DB_NAME = "autofill.sqlite"

/**
 * An implementation of [CreditCardsAddressesStorage] back by the application-services' `autofill`
 * library.
 */
@SuppressWarnings("TooManyFunctions")
class AutofillCreditCardsAddressesStorage(
    context: Context
) : CreditCardsAddressesStorage, AutoCloseable {

    private val coroutineContext by lazy { Dispatchers.IO }

    private val conn by lazy {
        AutofillStorageConnection.init(dbPath = context.getDatabasePath(AUTOFILL_DB_NAME).absolutePath)
        AutofillStorageConnection
    }

    override suspend fun addCreditCard(creditCardFields: UpdatableCreditCardFields): CreditCard =
        withContext(coroutineContext) {
            conn.getStorage().addCreditCard(creditCardFields.into()).into()
        }

    override suspend fun getCreditCard(guid: String): CreditCard = withContext(coroutineContext) {
        conn.getStorage().getCreditCard(guid).into()
    }

    override suspend fun getAllCreditCards(): List<CreditCard> = withContext(coroutineContext) {
        conn.getStorage().getAllCreditCards().map { it.into() }
    }

    override suspend fun updateCreditCard(
        guid: String,
        creditCardFields: UpdatableCreditCardFields
    ) = withContext(coroutineContext) {
        conn.getStorage().updateCreditCard(guid, creditCardFields.into())
    }

    override suspend fun deleteCreditCard(guid: String): Boolean = withContext(coroutineContext) {
        conn.getStorage().deleteCreditCard(guid)
    }

    override suspend fun touchCreditCard(guid: String) = withContext(coroutineContext) {
        conn.getStorage().touchCreditCard(guid)
    }

    override suspend fun addAddress(addressFields: UpdatableAddressFields): Address =
        withContext(coroutineContext) {
            conn.getStorage().addAddress(addressFields.into()).into()
        }

    override suspend fun getAddress(guid: String): Address = withContext(coroutineContext) {
        conn.getStorage().getAddress(guid).into()
    }

    override suspend fun getAllAddresses(): List<Address> = withContext(coroutineContext) {
        conn.getStorage().getAllAddresses().map { it.into() }
    }

    override suspend fun updateAddress(guid: String, address: UpdatableAddressFields) =
        withContext(coroutineContext) {
            conn.getStorage().updateAddress(guid, address.into())
        }

    override suspend fun deleteAddress(guid: String): Boolean = withContext(coroutineContext) {
        conn.getStorage().deleteAddress(guid)
    }

    override suspend fun touchAddress(guid: String) = withContext(coroutineContext) {
        conn.getStorage().touchAddress(guid)
    }

    override fun close() {
        coroutineContext.cancel()
        conn.close()
    }
}

/**
 * A singleton wrapping a [RustAutofillStorage] connection.
 */
internal object AutofillStorageConnection : Closeable {
    @GuardedBy("this")
    private var storage: RustAutofillStorage? = null

    internal fun init(dbPath: String = AUTOFILL_DB_NAME) = synchronized(this) {
        if (storage == null) {
            storage = RustAutofillStorage(dbPath)
        }
    }

    internal fun getStorage(): RustAutofillStorage = synchronized(this) {
        check(storage != null) { "must call init first" }
        return storage!!
    }

    override fun close() = synchronized(this) {
        check(storage != null) { "must call init first" }
        storage!!.destroy()
        storage = null
    }
}
