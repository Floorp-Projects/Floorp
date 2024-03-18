/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.arch.lifecycle

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver

/**
 * Calls [Lifecycle.addObserver] for a variable list of [LifecycleObserver]s.
 */
fun Lifecycle.addObservers(vararg observers: LifecycleObserver) = observers.forEach { addObserver(it) }
