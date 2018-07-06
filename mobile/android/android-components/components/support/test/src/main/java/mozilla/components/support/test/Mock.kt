/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test

import org.mockito.Mockito

/**
 * Dynamically create a mock object. This method is helpful when creating mocks of classes
 * using generics.
 *
 * Instead of:
 * <code>val foo = Mockito.mock(....Class of Bar<Baz>?...)<code>
 *
 * You can just use:
 * <code>val foo: Bar<Baz> = mock()</code>
 */
inline fun <reified T : Any> mock(): T = Mockito.mock(T::class.java)!!
