package org.mozilla.fenix.helpers

import androidx.test.espresso.IdlingRegistry
import androidx.test.rule.ActivityTestRule
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.idlingresource.AddonsInstallingIdlingResource

object IdlingResourceHelper {

    // Idling Resource to manage installing an addon
    fun registerAddonInstallingIdlingResource(activityTestRule: ActivityTestRule<HomeActivity>) {
        IdlingRegistry.getInstance().register(
            AddonsInstallingIdlingResource(
                activityTestRule.activity.supportFragmentManager
            )
        )
    }

    fun unregisterAddonInstallingIdlingResource(activityTestRule: ActivityTestRule<HomeActivity>) {
        IdlingRegistry.getInstance().unregister(
            AddonsInstallingIdlingResource(
                activityTestRule.activity.supportFragmentManager
            )
        )
    }
}
