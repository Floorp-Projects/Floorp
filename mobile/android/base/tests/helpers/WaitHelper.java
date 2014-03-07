/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests.helpers;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.Actions.EventExpecter;
import org.mozilla.gecko.tests.components.ToolbarComponent;
import org.mozilla.gecko.tests.UITestContext;
import org.mozilla.gecko.tests.UITestContext.ComponentType;

import com.jayway.android.robotium.solo.Condition;
import com.jayway.android.robotium.solo.Solo;

import java.util.regex.Pattern;

/**
 * Provides functionality related to waiting on certain events to happen.
 */
public final class WaitHelper {
    // TODO: Make public for when Solo.waitForCondition is used directly (i.e. do not want
    // assertion from waitFor)?
    private static final int DEFAULT_MAX_WAIT_MS = 5000;
    private static final int PAGE_LOAD_WAIT_MS = 10000;
    private static final int CHANGE_WAIT_MS = 10000;

    // TODO: via lucasr - Add ThrobberVisibilityChangeVerifier?
    private static final ChangeVerifier[] PAGE_LOAD_VERIFIERS = new ChangeVerifier[] {
        new ToolbarTitleTextChangeVerifier()
    };

    private static UITestContext sContext;
    private static Solo sSolo;
    private static Actions sActions;

    private static ToolbarComponent sToolbar;

    private WaitHelper() { /* To disallow instantiation. */ }

    protected static void init(final UITestContext context) {
        sContext = context;
        sSolo = context.getSolo();
        sActions = context.getActions();

        sToolbar = (ToolbarComponent) context.getComponent(ComponentType.TOOLBAR);
    }

    /**
     * Waits for the given {@link solo.Condition} using the default wait duration; will throw an
     * AssertionError if the duration is elapsed and the condition is not satisfied.
     */
    public static void waitFor(String message, final Condition condition) {
        message = "Waiting for " + message + ".";
        fAssertTrue(message, sSolo.waitForCondition(condition, DEFAULT_MAX_WAIT_MS));
    }

    /**
     * Waits for the given {@link solo.Condition} using the given wait duration; will throw an
     * AssertionError if the duration is elapsed and the condition is not satisfied.
     */
    public static void waitFor(String message, final Condition condition, final int waitMillis) {
        message = "Waiting for " + message + " with timeout " + waitMillis + ".";
        fAssertTrue(message, sSolo.waitForCondition(condition, waitMillis));
    }

    /**
     * Waits for the Gecko event declaring the page has loaded. Takes in and runs a Runnable
     * that will perform the action that will cause the page to load.
     */
    public static void waitForPageLoad(final Runnable initiatingAction) {
        fAssertNotNull("initiatingAction is not null", initiatingAction);

        // Some changes to the UI occur in response to the same event we listen to for when
        // the page has finished loading (e.g. a page title update). As such, we ensure this
        // UI state has changed before returning from this method; here we store the initial
        // state.
        final ChangeVerifier[] pageLoadVerifiers = PAGE_LOAD_VERIFIERS;
        for (final ChangeVerifier verifier : pageLoadVerifiers) {
            verifier.storeState();
        }

        // Wait for the page load and title changed event.
        final EventExpecter contentEventExpecter = sActions.expectGeckoEvent("DOMContentLoaded");
        final EventExpecter titleEventExpecter = sActions.expectGeckoEvent("DOMTitleChanged");

        initiatingAction.run();

        contentEventExpecter.blockForEventDataWithTimeout(PAGE_LOAD_WAIT_MS);
        contentEventExpecter.unregisterListener();
        titleEventExpecter.blockForEventDataWithTimeout(PAGE_LOAD_WAIT_MS);
        titleEventExpecter.unregisterListener();

        // Verify remaining state has changed.
        for (final ChangeVerifier verifier : pageLoadVerifiers) {
            // If we timeout, either the state is set to the same value (which is fine), or
            // the state has not yet changed. Since we can't be sure it will ever change, move
            // on and let the assertions fail if applicable.
            final boolean hasTimedOut = !sSolo.waitForCondition(new Condition() {
                @Override
                public boolean isSatisfied() {
                    return verifier.hasStateChanged();
                }
            }, CHANGE_WAIT_MS);

            sContext.dumpLog(verifier.getLogTag(),
                    (hasTimedOut ? "timed out." : "was satisfied."));
        }
    }

    /**
     * Implementations of this interface verify that the state of the test has changed from
     * the invocation of storeState to the invocation of hasStateChanged. A boolean will be
     * returned from hasStateChanged, indicating this change of status.
     */
    private static interface ChangeVerifier {
        public String getLogTag();

        /**
         * Stores the initial state of the system. This system state is used to diff against
         * the end state to determine if the system has changed. Since this is just a diff
         * (with a timeout), this method could potentially store state inconsistent with
         * what is visible to the user.
         */
        public void storeState();
        public boolean hasStateChanged();
    }

    private static class ToolbarTitleTextChangeVerifier implements ChangeVerifier {
        private static final String LOGTAG = ToolbarTitleTextChangeVerifier.class.getSimpleName();

        // A regex that matches the page title that shows up while the page is loading.
        private static final Pattern LOADING_PREFIX = Pattern.compile("[A-Za-z]{3,9}://");

        private CharSequence mOldTitleText;

        @Override
        public String getLogTag() {
            return LOGTAG;
        }

        @Override
        public void storeState() {
            mOldTitleText = sToolbar.getPotentiallyInconsistentTitle();
            sContext.dumpLog(LOGTAG, "stored title, \"" + mOldTitleText + "\".");
        }

        @Override
        public boolean hasStateChanged() {
            // TODO: Additionally, consider Solo.waitForText.
            // TODO: Robocop sleeps .5 sec between calls. Cache title view?
            final CharSequence title = sToolbar.getPotentiallyInconsistentTitle();

            // TODO: Handle the case where the URL is shown instead of page title by preference.
            // HACK: We want to wait until the title changes to the state a tester may assert
            // (e.g. the page title). However, the title is set to the URL before the title is
            // loaded from the server and set as the final page title; we ignore the
            // intermediate URL loading state here.
            final boolean isLoading = LOADING_PREFIX.matcher(title).lookingAt();
            final boolean hasStateChanged = !isLoading && !mOldTitleText.equals(title);

            if (hasStateChanged) {
                sContext.dumpLog(LOGTAG, "state changed to title, \"" + title + "\".");
            }
            return hasStateChanged;
        }
    }
}
