/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions

/**
 * Interface for exceptions view interactors. This interface is implemented by objects that want
 * to respond to user interaction on the [ExceptionsView].
 */
interface ExceptionsInteractor<T> {
    /**
     * Called whenever all exception items are deleted
     */
    fun onDeleteAll()

    /**
     * Called whenever one exception item is deleted
     */
    fun onDeleteOne(item: T)
}
