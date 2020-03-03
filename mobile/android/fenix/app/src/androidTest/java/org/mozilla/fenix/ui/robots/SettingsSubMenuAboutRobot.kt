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
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestHelper
import java.text.SimpleDateFormat
import java.time.LocalDateTime
import java.time.format.DateTimeFormatterBuilder
import java.time.temporal.ChronoField
import java.util.Date
import java.util.Calendar

/**
 * Implementation of Robot Pattern for the settings search sub menu.
 */
class SettingsSubMenuAboutRobot {

    fun verifyAboutFirefoxPreview() = assertFirefoxPreviewPage()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun assertFirefoxPreviewPage() {
    assertVersionNumber()
    assertProductCompany()
    assertCurrentTimestamp()
    assertWhatIsNewInFirefoxPreview()
    assertSupport()
    assertPrivacyNotice()
    assertKnowYourRights()
    assertLicensingInformation()
    assertLibrariesUsed()
}

private fun assertVersionNumber() {
    val context = InstrumentationRegistry.getInstrumentation().targetContext

    val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
    val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()

    val buildNVersion = "${packageInfo.versionName} (Build #$versionCode)\n"
    val componentsVersion =
        "${mozilla.components.Build.version}, ${mozilla.components.Build.gitHash}"
    val geckoVersion =
        org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION + "-" + org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID
    val asVersion = mozilla.components.Build.applicationServicesVersion

    onView(withId(R.id.about_text))
        .check(matches(withText(containsString(buildNVersion))))
        .check(matches(withText(containsString(componentsVersion))))
        .check(matches(withText(containsString(geckoVersion))))
        .check(matches(withText(containsString(asVersion))))
}

private fun assertProductCompany() {
    onView(withId(R.id.about_content))
        .check(matches(withText(containsString("Firefox Preview is produced by Mozilla."))))
}

private fun assertCurrentTimestamp() {
    onView(withId(R.id.build_date))
        .check(BuildDateAssertion.isDisplayedDateAccurate())
}

private fun assertWhatIsNewInFirefoxPreview() {
    TestHelper.scrollToElementByText("What’s new in Firefox Preview")
    onView(withText("What’s new in Firefox Preview"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())

    // This is used to wait for the webpage to fully load
    TestHelper.waitUntilObjectIsFound("org.mozilla.fenix.debug:id/mozac_browser_toolbar_title_view")

    onView(withId(R.id.mozac_browser_toolbar_title_view)).check(
        matches(
            withText(
                containsString("What's new in Firefox Preview")
            )
        )
    )
    Espresso.pressBack()
}

private fun assertSupport() {
    TestHelper.scrollToElementByText("Support")
    onView(withText("Support"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())

    // This is used to wait for the webpage to fully load
    TestHelper.waitUntilObjectIsFound("org.mozilla.fenix.debug:id/mozac_browser_toolbar_title_view")

    onView(withId(R.id.mozac_browser_toolbar_title_view)).check(
        matches(
            withText(
                containsString("Firefox Preview | Mozilla Support")
            )

        )
    )
    Espresso.pressBack()
}

private fun assertPrivacyNotice() {
    TestHelper.scrollToElementByText("Privacy Notice")
    onView(withText("Privacy notice"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())

    // This is used to wait for the webpage to fully load
    TestHelper.waitUntilObjectIsFound("org.mozilla.fenix.debug:id/mozac_browser_toolbar_title_view")

    onView(withId(R.id.mozac_browser_toolbar_title_view)).check(
        matches(
            withText(
                containsString("Firefox Privacy Notice")
            )

        )
    )
    Espresso.pressBack()
}

private fun assertKnowYourRights() {
    TestHelper.scrollToElementByText("Know your rights")
    onView(withText("Know your rights"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())

    // This is used to wait for the webpage to fully load
    TestHelper.waitUntilObjectIsFound("org.mozilla.fenix.debug:id/mozac_browser_toolbar_title_view")

    onView(withId(R.id.mozac_browser_toolbar_title_view)).check(
        matches(
            withText(
                containsString("Firefox Preview - Your Rights | How to | Mozilla Support")
            )
        )
    )
    Espresso.pressBack()
}

private fun assertLicensingInformation() {
    TestHelper.scrollToElementByText("Libraries that we use")
    onView(withText("Licensing information"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())

    // This is used to wait for the webpage to fully load
    TestHelper.waitUntilObjectIsFound("org.mozilla.fenix.debug:id/mozac_browser_toolbar_title_view")

    onView(withId(R.id.mozac_browser_toolbar_title_view)).check(
        matches(
            withText(
                containsString("Licenses")
            )
        )
    )
    Espresso.pressBack()
}

private fun assertLibrariesUsed() {
    TestHelper.scrollToElementByText("Libraries that we use")
    onView(withText("Libraries that we use"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())

    onView(withId(R.id.action_bar)).check(matches(hasDescendant(withText(containsString("Firefox Preview | OSS Libraries")))))
    Espresso.pressBack()
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(withContentDescription("Navigate up")))

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
                if (!date.isWithinRangeOf(hours)) throw AssertionError("The build date is not within Range.")
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
                hours * -2
            ) // Gets the minDate by subtracting from maxDate
            val minDate = calendar.time
            return updatedDate.after(minDate) && updatedDate.before(maxDate)
        }

        private fun LocalDateTime.isWithinRangeOf(
            hours: Int,
            baselineDate: LocalDateTime
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
