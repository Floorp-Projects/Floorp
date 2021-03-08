/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill

import android.os.Build
import android.os.CancellationSignal
import android.service.autofill.AutofillService
import android.service.autofill.FillCallback
import android.service.autofill.FillRequest
import android.service.autofill.SaveCallback
import android.service.autofill.SaveRequest
import androidx.annotation.RequiresApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.feature.autofill.handler.FillRequestHandler
import mozilla.components.feature.autofill.structure.toRawStructure

/**
 * Service responsible for implementing Android's Autofill framework.
 */
@RequiresApi(Build.VERSION_CODES.O)
abstract class AbstractAutofillService : AutofillService() {
    abstract val configuration: AutofillConfiguration

    private val fillHandler by lazy { FillRequestHandler(context = this, configuration) }

    override fun onFillRequest(
        request: FillRequest,
        cancellationSignal: CancellationSignal,
        callback: FillCallback
    ) {
        // We are using GlobalScope here instead of a scope bound to the service since the service
        // seems to get destroyed before we invoke a method on the callback. So we need a scope that
        // lives longer than the service.
        GlobalScope.launch(Dispatchers.IO) {
            // You may be wondering why we translate the AssistStructure into a RawStructure and then
            // create a FillResponseBuilder that outputs the FillResponse. This is purely for testing.
            // Neither AssistStructure nor FillResponse can be created by us and they do not let us
            // inspect their data. So we create these intermediate objects that we can create and
            // inspect in unit tests.
            val structure = request.fillContexts.last().structure.toRawStructure()
            val responseBuilder = fillHandler.handle(structure)
            val response = responseBuilder?.build(this@AbstractAutofillService, configuration)
            callback.onSuccess(response)
        }
    }

    override fun onSaveRequest(request: SaveRequest, callback: SaveCallback) {
        // This callback should not get invoked since we do not indicate that we are interested in
        // saving any data (yet). If for whatever reason it does get invoked then we pretent that
        // we handled the request successfully. Calling onFailure() requires to pass in a message
        // and on Android systems before Q this message may be shown in a toast.
        callback.onSuccess()
    }
}
