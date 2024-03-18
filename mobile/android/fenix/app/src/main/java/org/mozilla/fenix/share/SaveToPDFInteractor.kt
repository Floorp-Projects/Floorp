/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

/**
 * Callbacks for possible user interactions on the [SaveToPDFItem]
 */
interface SaveToPDFInteractor {
    /**
     * Generates a PDF from the given [tabId].
     * @param tabId The ID of the tab to save as PDF.
     */
    fun onSaveToPDF(tabId: String?)

    /**
     * Prints from the given [tabId].
     * @param tabId The ID of the tab to print.
     */
    fun onPrint(tabId: String?)
}
