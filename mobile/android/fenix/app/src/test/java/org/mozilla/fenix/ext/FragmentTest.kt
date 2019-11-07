package org.mozilla.fenix.ext

import mozilla.components.support.test.robolectric.testContext
import org.mozilla.fenix.TestApplication
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.mockkStatic
import io.mockk.verify
import io.mockk.every
import io.mockk.Runs
import io.mockk.just
import io.mockk.confirmVerified
import androidx.fragment.app.Fragment
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.fragment.NavHostFragment.findNavController
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.NavDestination
import androidx.navigation.NavController
import androidx.navigation.Navigator.Extras
import org.junit.Before

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)

class FragmentTest {

    val navDirections: NavDirections = mockk(relaxed = true)
    val mockDestination = spyk(NavDestination("hi"))
    val mockExtras: Extras = mockk(relaxed = true)
    val mockId = 4
    val navController = spyk(NavController(testContext))
    val mockFragment: Fragment = mockk(relaxed = true)
    val mockOptions: NavOptions = mockk(relaxed = true)

    @Before
    fun setup() {
        mockkStatic(NavHostFragment::class)
        every { (NavHostFragment.findNavController(mockFragment)) } returns navController
        every { (NavHostFragment.findNavController(mockFragment).getCurrentDestination()) } returns mockDestination
        every { (mockDestination.getId()) } returns mockId
        every { (navController.getCurrentDestination()) } returns mockDestination
        every { (NavHostFragment.findNavController(mockFragment).getCurrentDestination()?.getId()) } answers { (mockDestination.getId()) }
    }

    @Test
    fun `Test nav fun with ID and directions`() {
        every { (NavHostFragment.findNavController(mockFragment).navigate(navDirections)) } just Runs

        mockFragment.nav(mockId, navDirections)
        verify { (NavHostFragment.findNavController(mockFragment).getCurrentDestination()) }
        verify { (NavHostFragment.findNavController(mockFragment).navigate(navDirections)) }
        confirmVerified(mockFragment)
    }

    @Test
    fun `Test nav fun with ID, directions, and extras`() {
        every { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, mockExtras)) } just Runs

        mockFragment.nav(mockId, navDirections, mockExtras)
        verify { (NavHostFragment.findNavController(mockFragment).getCurrentDestination()) }
        verify { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, mockExtras)) }
        confirmVerified(mockFragment)
    }

    @Test
    fun `Test nav fun with ID, directions, and options`() {
        every { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, mockOptions)) } just Runs

        mockFragment.nav(mockId, navDirections, mockOptions)
        verify { (NavHostFragment.findNavController(mockFragment).getCurrentDestination()) }
        verify { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, mockOptions)) }
        confirmVerified(mockFragment)
    }
}
