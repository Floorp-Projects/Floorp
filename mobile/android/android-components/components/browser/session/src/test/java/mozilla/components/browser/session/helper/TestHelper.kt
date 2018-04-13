/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.helper

import org.mockito.Mockito

/**
 * Dynamically create a mock object. This method is helpful when creating mocks of classes using
 * generics.
 */
inline fun <reified T : Any> mock(): T = Mockito.mock(T::class.java)!!