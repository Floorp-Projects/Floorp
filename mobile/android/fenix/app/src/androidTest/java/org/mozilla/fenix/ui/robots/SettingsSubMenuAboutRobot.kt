/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.os.Build
import android.widget.TextView
import androidx.core.content.pm.PackageInfoCompat
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewAssertion
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LISTS_MAXSWIPES
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.settings.SupportUtils
import java.text.SimpleDateFormat
import java.time.LocalDateTime
import java.time.format.DateTimeFormatterBuilder
import java.time.temporal.ChronoField
import java.util.Calendar
import java.util.Date

/**
 * Implementation of Robot Pattern for the settings search sub menu.
 */
class SettingsSubMenuAboutRobot {
    fun verifyAboutFirefoxPreviewInfo() {
        assertVersionNumber()
        assertProductCompany()
        assertCurrentTimestamp()
        verifyTheLinksList()
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun navigateBackToAboutPage(itemToInteract: () -> Unit) {
    navigationToolbar {
    }.openThreeDotMenu {
    }.openSettings {
    }.openAboutFirefoxPreview {
        itemToInteract()
    }
}

private fun verifyTheLinksList() {
    assertAboutToolbar()
    assertWhatIsNewInFirefoxPreview()
    navigateBackToAboutPage(::assertSupport)
    assertCrashes()
    navigateBackToAboutPage(::assertPrivacyNotice)
    navigateBackToAboutPage(::assertKnowYourRights)
    navigateBackToAboutPage(::assertLicensingInformation)
    navigateBackToAboutPage(::assertLibrariesUsed)
}

private fun assertAboutToolbar() =
    onView(
        allOf(
            withId(R.id.navigationToolbar),
            hasDescendant(withText("About $appName")),
        ),
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertVersionNumber() {
    val context = InstrumentationRegistry.getInstrumentation().targetContext

    val packageInfo = context.packageManager.getPackageInfoCompat(context.packageName, 0)
    val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()
    val buildNVersion = "${packageInfo.versionName} (Build #$versionCode)\n"
    val geckoVersion =
        org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION + "-" + org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID
    val asVersion = mozilla.components.Build.applicationServicesVersion

    onView(withId(R.id.about_text))
        .check(matches(withText(containsString(buildNVersion))))
        .check(matches(withText(containsString(geckoVersion))))
        .check(matches(withText(containsString(asVersion))))
}

private fun assertProductCompany() {
    onView(withId(R.id.about_content))
        .check(matches(withText(containsString("$appName is produced by Mozilla."))))
}

private fun assertCurrentTimestamp() {
    onView(withId(R.id.build_date))
        // Currently UI tests run against debug builds, which display a hard-coded string 'debug build'
        // instead of the date. See https://github.com/mozilla-mobile/fenix/pull/10812#issuecomment-633746833
        .check(matches(withText(containsString("debug build"))))
    // This assertion should be valid for non-debug build types.
    // .check(BuildDateAssertion.isDisplayedDateAccurate())
}

private fun assertWhatIsNewInFirefoxPreview() {
    aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)

    onView(withText("What’s new in $appName"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())
}

private fun assertSupport() {
    aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)

    onView(withText("Support"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())

    TestHelper.verifyUrl(
        "support.mozilla.org",
        "org.mozilla.fenix.debug:id/mozac_browser_toolbar_url_view",
        R.id.mozac_browser_toolbar_url_view,
    )
}

private fun assertCrashes() {
    navigationToolbar {
    }.openThreeDotMenu {
    }.openSettings {
    }.openAboutFirefoxPreview {}

    aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)

    onView(withText("Crashes"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())

    assertItemContainingTextExists(itemContainingText("No crash reports have been submitted."))

    for (i in 1..3) {
        Espresso.pressBack()
    }
}

private fun assertPrivacyNotice() {
    aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)

    onView(withText("Privacy notice"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())

    TestHelper.verifyUrl(
        "/privacy/firefox",
        "org.mozilla.fenix.debug:id/mozac_browser_toolbar_url_view",
        R.id.mozac_browser_toolbar_url_view,
    )
}

private fun assertKnowYourRights() {
    aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)

    onView(withText("Know your rights"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())

    TestHelper.verifyUrl(
        SupportUtils.SumoTopic.YOUR_RIGHTS.topicStr,
        "org.mozilla.fenix.debug:id/mozac_browser_toolbar_url_view",
        R.id.mozac_browser_toolbar_url_view,
    )
}

private fun assertLicensingInformation() {
    aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)

    onView(withText("Licensing information"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())

    TestHelper.verifyUrl(
        "about:license",
        "org.mozilla.fenix.debug:id/mozac_browser_toolbar_url_view",
        R.id.mozac_browser_toolbar_url_view,
    )
}

private fun assertLibrariesUsed() {
    aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)

    onView(withText("Libraries that we use"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        .perform(click())

    onView(withId(R.id.navigationToolbar)).check(matches(hasDescendant(withText(containsString("$appName | OSS Libraries")))))
    Espresso.pressBack()
}

private val aboutMenuList = UiScrollable(UiSelector().resourceId("$packageName:id/about_layout"))

private fun goBackButton() =
    onView(withContentDescription("Navigate up"))

class BuildDateAssertion {
    // When the app is built on firebase, there are times where the BuildDate is off by a few seconds or a few minutes.
    // To compensate for that slight discrepancy, this assertion was added to see if the Build Date shown
    // is within a reasonable amount of time from when the app was built.
    companion object {
        // this pattern represents the following date format: "Monday 12/30 @ 6:49 PM"
        private const val DATE_PATTERN = "EEEE M/d @ h:m a"

        //
        private const val NUM_OF_HOURS = 1

        fun isDisplayedDateAccurate(): ViewAssertion {
            return ViewAssertion { view, noViewFoundException ->
                if (noViewFoundException != null) throw noViewFoundException

                val textFromView = (view as TextView).text
                    ?: throw AssertionError("This view is not of type TextView")

                verifyDateIsWithinRange(textFromView.toString(), NUM_OF_HOURS)
            }
        }

        private fun verifyDateIsWithinRange(dateText: String, hours: Int) {
            // This assertion checks whether has defined a range of tim
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
                val simpleDateFormat = SimpleDateFormat(DATE_PATTERN)
                val date = simpleDateFormat.parse(dateText)
                if (date == null || !date.isWithinRangeOf(hours)) {
                    throw AssertionError("The build date is not within Range.")
                }
            } else {
                val textviewDate = getLocalDateTimeFromString(dateText)
                val buildConfigDate = getLocalDateTimeFromString(BuildConfig.BUILD_DATE)

                if (!buildConfigDate.isEqual(textviewDate) &&
                    !textviewDate.isWithinRangeOf(hours, buildConfigDate)
                ) {
                    throw AssertionError("$textviewDate is not equal to the date within the build config: $buildConfigDate, and are not within a reasonable amount of time from each other.")
                }
            }
        }

        private fun Date.isWithinRangeOf(hours: Int): Boolean {
            // To determine the date range, the maxDate is retrieved by adding the variable hours to the calendar.
            // Since the calendar will represent the maxDate at this time, to retrieve the minDate the variable hours is multipled by negative 2 and added to the calendar
            // This will result in the maxDate being equal to the original Date + hours, and minDate being equal to original Date - hours

            val calendar = Calendar.getInstance()
            val currentYear = calendar.get(Calendar.YEAR)
            calendar.time = this
            calendar.set(Calendar.YEAR, currentYear)
            val updatedDate = calendar.time

            calendar.add(Calendar.HOUR_OF_DAY, hours)
            val maxDate = calendar.time
            calendar.add(
                Calendar.HOUR_OF_DAY,
                hours * -2,
            ) // Gets the minDate by subtracting from maxDate
            val minDate = calendar.time
            return updatedDate.after(minDate) && updatedDate.before(maxDate)
        }

        private fun LocalDateTime.isWithinRangeOf(
            hours: Int,
            baselineDate: LocalDateTime,
        ): Boolean {
            val upperBound = baselineDate.plusHours(hours.toLong())
            val lowerBound = baselineDate.minusHours(hours.toLong())
            val currentDate = this
            return currentDate.isAfter(lowerBound) && currentDate.isBefore(upperBound)
        }

        private fun getLocalDateTimeFromString(buildDate: String): LocalDateTime {
            val dateFormatter = DateTimeFormatterBuilder().appendPattern(DATE_PATTERN)
                .parseDefaulting(ChronoField.YEAR, LocalDateTime.now().year.toLong())
                .toFormatter()
            return LocalDateTime.parse(buildDate, dateFormatter)
        }
    }
}
