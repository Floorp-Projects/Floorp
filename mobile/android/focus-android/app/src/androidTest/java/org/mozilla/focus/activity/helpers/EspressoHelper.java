/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.helpers;

import android.support.annotation.NonNull;
import android.support.test.espresso.ViewInteraction;

import org.mozilla.focus.R;

import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.pressImeActionButton;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasFocus;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

/**
 * Some convenient methods for testing Focus with espresso.
 */
public class EspressoHelper {
    public static void navigateToWebsite(@NonNull String input) {
        onView(withId(R.id.url_edit))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(typeText(input), pressImeActionButton());
    }

    public static void navigateToMockWebServer(@NonNull MockWebServer webServer, String path) {
        navigateToWebsite(webServer.url(path).toString());
    }

    public static ViewInteraction onFloatingEraseButton() {
        return onView(withId(R.id.erase));
    }

    public static ViewInteraction onFloatingTabsButton() {
        return onView(withId(R.id.tabs));
    }
}
