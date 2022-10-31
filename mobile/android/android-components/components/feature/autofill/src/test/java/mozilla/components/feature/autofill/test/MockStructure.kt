/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.test

import android.view.autofill.AutofillId
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.feature.autofill.handler.FillRequestHandlerTest
import mozilla.components.feature.autofill.structure.AutofillNodeNavigator
import mozilla.components.feature.autofill.structure.RawStructure
import java.io.File

@ExperimentalCoroutinesApi
internal fun FillRequestHandlerTest.createMockStructure(filename: String, packageName: String): RawStructure {
    val classLoader = javaClass.classLoader ?: throw RuntimeException("No class loader")
    val resource = classLoader.getResource(filename) ?: throw RuntimeException("Resource not found")
    val file = File(resource.path)

    return MockStructure(packageName, file)
}

private class MockStructure(
    private val packageName: String,
    private val file: File,
) : RawStructure {
    override val activityPackageName: String = packageName

    override fun createNavigator(): AutofillNodeNavigator<*, AutofillId> {
        return DOMNavigator(file, packageName)
    }
}
