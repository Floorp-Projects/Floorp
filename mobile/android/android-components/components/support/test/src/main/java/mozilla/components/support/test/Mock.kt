/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test

import android.view.MotionEvent
import org.mockito.Mockito

/**
 * Dynamically create a mock object. This method is helpful when creating mocks of classes
 * using generics.
 *
 * Optional @param setup will be called on the mock after init.
 *
 * Instead of:
 * <code>val foo = Mockito.mock(....Class of Bar<Baz>?...)<code>
 *
 * You can just use:
 * <code>val foo: Bar<Baz> = mock()</code>
 */
inline fun <reified T : Any> mock(noinline setup: (T.() -> Unit)? = null): T = Mockito.mock(T::class.java)!!
    .apply { setup?.invoke(this) }

/**
 * Equivalent to [mock] but allows inline setup of suspending functions.
 */
suspend inline fun <reified T : Any> coMock(noinline setup: (suspend T.() -> Unit)? = null): T = Mockito.mock(T::class.java)!!
    .apply { setup?.invoke(this) }

/**
 * Enables stubbing methods. Use it when you want the mock to return particular value when particular method is called.
 *
 * Alias for [Mockito.when ].
 *
 * Taken from [mockito-kotlin](https://github.com/nhaarman/mockito-kotlin/).
 */
@Suppress("NOTHING_TO_INLINE")
inline fun <T> whenever(methodCall: T) = Mockito.`when`(methodCall)!!

/**
 * Creates a custom [MotionEvent] for testing. As of SDK 28 [MotionEvent]s can't be mocked anymore and need to be created
 * through [MotionEvent.obtain].
 */
fun mockMotionEvent(
    action: Int,
    downTime: Long = System.currentTimeMillis(),
    eventTime: Long = System.currentTimeMillis(),
    x: Float = 0f,
    y: Float = 0f,
    metaState: Int = 0,
): MotionEvent {
    return MotionEvent.obtain(downTime, eventTime, action, x, y, metaState)
}
