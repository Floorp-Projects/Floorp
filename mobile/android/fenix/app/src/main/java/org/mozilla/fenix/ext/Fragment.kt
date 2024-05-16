/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.app.Activity
import android.content.Intent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.IdRes
import androidx.annotation.StringRes
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.fragment.findNavController
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.support.utils.ext.isLandscape
import org.mozilla.fenix.NavHostActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.components.toolbar.navbar.ToolbarContainerView

/**
 * Get the requireComponents of this application.
 */
val Fragment.requireComponents: Components
    get() = requireContext().components

fun Fragment.nav(@IdRes id: Int?, directions: NavDirections, options: NavOptions? = null) {
    findNavController().nav(id, directions, options)
}

fun Fragment.getPreferenceKey(@StringRes resourceId: Int): String = getString(resourceId)

/**
 * Displays the activity toolbar with the given [title].
 * Throws if the fragment is not attached to an [AppCompatActivity].
 */
fun Fragment.showToolbar(title: String) {
    (requireActivity() as AppCompatActivity).title = title
    activity?.setNavigationIcon(R.drawable.ic_back_button)
    (activity as NavHostActivity).getSupportActionBarAndInflateIfNecessary().show()
}

/**
 * Run the [block] only if the [Fragment] is attached.
 *
 * @param block A callback to be executed if the container [Fragment] is attached.
 */
internal inline fun Fragment.runIfFragmentIsAttached(block: () -> Unit) {
    context?.let {
        block()
    }
}

/**
 * Hides the activity toolbar.
 * Throws if the fragment is not attached to an [AppCompatActivity].
 */
fun Fragment.hideToolbar() {
    (requireActivity() as AppCompatActivity).supportActionBar?.hide()
}

/**
 * Pops the backstack to force users to re-auth if they put the app in the background and return to
 * it while being inside a secured flow (e.g. logins or credit cards).
 *
 * Does nothing if the user is currently navigating to any of the [destinations] given as a
 * parameter.
 */
fun Fragment.redirectToReAuth(
    destinations: List<Int>,
    currentDestination: Int?,
    currentLocation: Int,
) {
    if (currentDestination !in destinations) {
        // Workaround for memory leak caused by Android SDK bug
        // https://issuetracker.google.com/issues/37125819
        activity?.invalidateOptionsMenu()
        when (currentLocation) {
            R.id.loginDetailFragment,
            R.id.editLoginFragment,
            R.id.addLoginFragment,
            R.id.savedLoginsFragment,
            -> {
                findNavController().popBackStack(R.id.savedLoginsAuthFragment, false)
            }
            R.id.creditCardEditorFragment,
            R.id.creditCardsManagementFragment,
            -> {
                findNavController().popBackStack(R.id.autofillSettingFragment, false)
            }
        }
    }
}

fun Fragment.breadcrumb(
    message: String,
    data: Map<String, String> = emptyMap(),
) {
    val activityName = activity?.let { it::class.java.simpleName } ?: "null"

    requireComponents.analytics.crashReporter.recordCrashBreadcrumb(
        Breadcrumb(
            category = this::class.java.simpleName,
            message = message,
            data = data + mapOf(
                "instance" to hashCode().toString(),
                "activityInstance" to activity?.hashCode().toString(),
                "activityName" to activityName,
            ),
            level = Breadcrumb.Level.INFO,
        ),
    )
}

/**
 * Sets the [WindowManager.LayoutParams.FLAG_SECURE] flag for the current activity window.
 */
fun Fragment.secure() {
    this.activity?.window?.addFlags(
        WindowManager.LayoutParams.FLAG_SECURE,
    )
}

/**
 * Clears the [WindowManager.LayoutParams.FLAG_SECURE] flag for the current activity window.
 */
fun Fragment.removeSecure() {
    this.activity?.window?.clearFlags(
        WindowManager.LayoutParams.FLAG_SECURE,
    )
}

/**
 * Register a request to start an activity for result.
 */
fun Fragment.registerForActivityResult(
    onFailure: (result: ActivityResult) -> Unit = {},
    onSuccess: (result: ActivityResult) -> Unit,
): ActivityResultLauncher<Intent> {
    return registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            onSuccess(result)
        } else {
            onFailure(result)
        }
    }
}

/**
 *  Checks whether the current fragment is running on a tablet.
 */
fun Fragment.isTablet(): Boolean {
    return resources.getBoolean(R.bool.tablet)
}

/**
 *
 * Manages the state of the NavBar upon an orientation change.
 *
 * @param parent The top level [ViewGroup] of the fragment, which will be hosting toolbar/navbar container.
 * @param toolbarView [View] responsible for showing the AddressBar.
 * @param bottomToolbarContainerView The [ToolbarContainerView] hosting the NavBar.
 * @param reinitializeNavBar lambda for re-initializing the NavBar inside the host [Fragment].
 */
fun Fragment.updateNavBarForConfigurationChange(
    parent: ViewGroup,
    toolbarView: View,
    bottomToolbarContainerView: ToolbarContainerView?,
    reinitializeNavBar: () -> Unit,
) {
    if (requireContext().isLandscape()) {
        // In landscape mode we want to remove the navigation bar.
        parent.removeView(bottomToolbarContainerView)

        // If address bar was positioned at bottom and we have removed the toolbar container, we are adding address bar
        // back.
        val isToolbarAtBottom = requireComponents.settings.toolbarPosition == ToolbarPosition.BOTTOM

        // Toolbar already having a parent is an edge case, but it could happen if configurationChange is called after
        // onCreateView with the same orientation. Caught it on a foldable emulator while going from single screen
        // portrait mode to landscape tablet, back and forth.
        val hasParent = toolbarView.parent != null
        if (isToolbarAtBottom && !hasParent) {
            parent.addView(toolbarView)
        }
    } else {
        // Already having a bottomContainer after switching back to portrait mode will happen when address bar is
        // positioned at bottom and also as an edge case if configurationChange is called after onCreateView with the
        // same orientation. Caught it on a foldable emulator while going from single screen portrait mode to landscape
        // table, back and forth.
        bottomToolbarContainerView?.let {
            parent.removeView(it)
        }

        reinitializeNavBar()
    }
}

/**
 *
 * Manages the state of the microsurvey prompt on orientation change.
 *
 * @param parent The top level [ViewGroup] of the fragment, which will be hosting the [bottomToolbarContainerView].
 * @param bottomToolbarContainerView The [ToolbarContainerView] hosting the microsurvey prompt.
 * @param reinitializeMicrosurveyPrompt lambda for re-initializing the microsurvey prompt inside the host [Fragment].
 */
fun Fragment.updateMicrosurveyPromptForConfigurationChange(
    parent: ViewGroup,
    bottomToolbarContainerView: ToolbarContainerView?,
    reinitializeMicrosurveyPrompt: () -> Unit,
) {
    if (!requireContext().isLandscape()) {
        // Already having a bottomContainer after switching back to portrait mode will happen when address bar is
        // positioned at bottom and also as an edge case if configurationChange is called after onCreateView with the
        // same orientation. Observed on a foldable emulator while going from single screen portrait mode to landscape
        // tablet, back and forth.
        bottomToolbarContainerView?.let {
            parent.removeView(it)
        }

        reinitializeMicrosurveyPrompt()
    }
}
