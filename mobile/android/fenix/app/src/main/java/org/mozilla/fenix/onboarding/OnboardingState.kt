/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

/**
 * Describes various onboarding states.
 */
sealed class OnboardingState {
    /**
     * Signed out, without an option to auto-login using a shared FxA account.
     */
    object SignedOutNoAutoSignIn : OnboardingState()

    /**
     * Signed in.
     */
    object SignedIn : OnboardingState()
}
