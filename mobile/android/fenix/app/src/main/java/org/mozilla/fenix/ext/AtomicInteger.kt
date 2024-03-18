/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import java.util.concurrent.atomic.AtomicInteger

/**
 *  Increases an AtomicInteger safely.
 */
fun AtomicInteger.getAndIncrementNoOverflow() {
    var prev: Int
    var next: Int
    do {
        prev = this.get()
        next = if (prev == Integer.MAX_VALUE) prev else prev + 1
    } while (!this.compareAndSet(prev, next))
}
