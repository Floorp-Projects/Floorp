/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.Visibility

class HomeScreenRobot {
    fun verifyHomeScreen() = homeScreen()
    fun verifyHomePrivateBrowsingButton() = homePrivateBrowsingButton()
    fun verifyHomeMenu() = homeMenu()
    fun verifyHomeWordmark() = homeWordmark()
    fun verifyHomeToolbar() = homeToolbar()
    fun verifyHomeComponent() = homeComponent()
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit) {
    HomeScreenRobot().interact()
}

private fun homeScreen() = onView(ViewMatchers.withResourceName("homeLayout"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun homePrivateBrowsingButton() = onView(ViewMatchers.withResourceName("privateBrowsingButton"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun homeMenu() = onView(ViewMatchers.withResourceName("menuButton"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun homeWordmark() = onView(ViewMatchers.withResourceName("wordmark"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun homeToolbar() = onView(ViewMatchers.withResourceName("toolbar"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun homeComponent() = onView(ViewMatchers.withResourceName("home_component"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
