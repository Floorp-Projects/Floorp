/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

/**
 * Thrown when a collection has been modified by another client while we were either
 * downloading from it or uploading to it.
 *
 * @author grisha
 */
public class CollectionConcurrentModificationException extends ReflowIsNecessaryException {
    private static final long serialVersionUID = 2701457832508838524L;
}
