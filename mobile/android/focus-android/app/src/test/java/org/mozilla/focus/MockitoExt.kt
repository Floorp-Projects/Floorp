/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import org.mockito.ArgumentCaptor
import org.mockito.Mockito

fun <T> any(): T = Mockito.any()

inline fun <reified T> argumentCaptor(): ArgumentCaptor<T> = ArgumentCaptor.forClass(T::class.java)
