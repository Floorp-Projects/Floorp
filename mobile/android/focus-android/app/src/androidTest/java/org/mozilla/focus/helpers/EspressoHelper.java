/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.helpers;

import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.test.espresso.ViewInteraction;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.mozilla.focus.R;

import okhttp3.mockwebserver.MockWebServer;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasFocus;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.endsWith;

/**
 * Some convenient methods for testing Focus with espresso.
 */
public class EspressoHelper {
    public static void openSettings() {
        openMenu();

        onView(withId(R.id.settings))
                .check(matches(isDisplayed()))
                .perform(click());

        assertToolbarMatchesText(R.string.menu_settings);
    }

    public static void openMenu() {
        onView(withId(R.id.menuView))
                .check(matches(isDisplayed()))
                .perform(click());
    }

    public static void navigateToWebsite(@NonNull String input) {
        onView(withId(R.id.urlView))
                .check(matches(isDisplayed()))
                .check(matches(hasFocus()))
                .perform(replaceText(input), pressImeActionButton());
    }

    public static void assertToolbarMatchesText(@StringRes int titleResource) {
        onView(allOf(withClassName(endsWith("TextView")), withParent(withId(R.id.toolbar))))
                .check(matches(isDisplayed()))
                .check(matches(withText(titleResource)));
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

    public static Matcher<View> childAtPosition(
            final Matcher<View> parentMatcher, final int position) {

        return new TypeSafeMatcher<View>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("Child at position " + position + " in parent ");
                parentMatcher.describeTo(description);
            }

            @Override
            public boolean matchesSafely(View view) {
                ViewParent parent = view.getParent();
                return parent instanceof ViewGroup && parentMatcher.matches(parent)
                        && view.equals(((ViewGroup) parent).getChildAt(position));
            }
        };
    }
}
