/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("MatchingDeclarationName")

package mozilla.components.concept.push.exceptions

/**
 * Signals that a subscription method has been invoked at an illegal or inappropriate time.
 *
 * See also [Exception].
 */
class SubscriptionException(
    override val message: String? = null,
    override val cause: Throwable? = null,
) : Exception()
