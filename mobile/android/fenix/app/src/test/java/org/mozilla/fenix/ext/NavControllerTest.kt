package org.mozilla.fenix.ext

import android.os.Bundle
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.Navigator.Extras
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)

class NavControllerTest {

    val navController: NavController = mockk(relaxed = true)
    val navDirections = mockk<NavDirections>(relaxed = true)
    val mockDestination: NavDestination = mockk(relaxed = true)
    val mockExtras: Extras = mockk(relaxed = true)
    val mockOptions: NavOptions = mockk(relaxed = true)
    val mockBundle: Bundle = mockk(relaxed = true)

    @Test
    fun `Nav with id and directions args`() {
        every { (navController.getCurrentDestination()) } returns mockDestination
        every { (mockDestination.getId()) } returns 4
        navController.nav(4, navDirections)
        verify { (navController.getCurrentDestination()) }
        verify { (navController.navigate(navDirections)) }
    }

    @Test
    fun `Nav with id, directions, and extras args`() {
        every { (navController.getCurrentDestination()) } returns mockDestination
        every { (mockDestination.getId()) } returns 4
        navController.nav(4, navDirections, mockExtras)
        verify { (navController.getCurrentDestination()) }
        verify { (navController.navigate(navDirections, mockExtras)) }
    }

    @Test
    fun `Nav with id, directions, and options args`() {
        every { (navController.getCurrentDestination()) } returns mockDestination
        every { (mockDestination.getId()) } returns 4
        navController.nav(4, navDirections, mockOptions)
        verify { (navController.getCurrentDestination()) }
        verify { (navController.navigate(navDirections, mockOptions)) }
    }

    @Test
    fun `Nav with id, destId, bundle, options, and extras args`() {
        every { (navController.getCurrentDestination()) } returns mockDestination
        every { (mockDestination.getId()) } returns 4
        navController.nav(4, 5, mockBundle, mockOptions, mockExtras)
        verify { (navController.getCurrentDestination()) }
        verify { (navController.navigate(5, mockBundle, mockOptions, mockExtras)) }
    }

    @Test
    fun `Test error response for id exception in-block`() {
        every { (navController.getCurrentDestination()) } returns mockDestination
        every { (mockDestination.getId()) } returns 4
        navController.nav(7, navDirections)
        verify { (recordIdException(mockDestination.getId(), 7)) }
    }

    // TO-DO Not Working
    /* @Test
    fun `Test record id exception fun`() {
        mockkStatic(String::class)
        val actual = 7
        var expected = 4

        class MySentry() {
            public fun capture(myString: String) {
                println(myString) }
        }

        class MyBuildConfig() {
            public val SENTRY_TOKEN = "Mozilla"
        }

        //val Sentry = MySentry()
        val BuildConfig = MyBuildConfig()
        every {BuildConfig.SENTRY_TOKEN.isNullOrEmpty()} returns false
        recordIdException(actual, expected)
        //verify { Sentry.capture("Fragment id $actual did not match expected $expected")}
    }*/
}
