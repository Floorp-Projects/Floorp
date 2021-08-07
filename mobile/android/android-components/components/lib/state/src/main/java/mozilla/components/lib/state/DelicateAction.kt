/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

/**
 *
 * Marks an [Action] in the [Store] that are **delicate** &mdash;
 * they have limited use-case and shall ve used with care in general code.
 * Any use of a delicate declaration has to be carefully reviewed to make sure it is
 * properly used and is not used for non-debugging or testing purposes.
 * Carefully read documentation of any declaration marked as `DelicateAction`.
 */
@MustBeDocumented
@Retention(value = AnnotationRetention.BINARY)
@RequiresOptIn(
    level = RequiresOptIn.Level.WARNING,
    message = "This is a delicate Action and should only be used for situations that require debugging or testing." +
        " Make sure you fully read and understand documentation of the action that is marked as a delicate Action."
)
@Target(AnnotationTarget.CLASS)
public annotation class DelicateAction
