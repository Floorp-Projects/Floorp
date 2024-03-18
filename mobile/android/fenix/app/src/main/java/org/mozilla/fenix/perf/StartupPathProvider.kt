/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.NONE
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner

/**
 * The "path" that this activity started in. See the
 * [Fenix perf glossary](https://wiki.mozilla.org/index.php?title=Performance/Fenix/Glossary)
 * for specific definitions.
 *
 * This should be a member variable of [Activity] because its data is tied to the lifecycle of an
 * Activity. Call [attachOnActivityOnCreate] & [onIntentReceived] for this class to work correctly.
 */
class StartupPathProvider {

    /**
     * The path the application took to
     * [Fenix perf glossary](https://wiki.mozilla.org/index.php?title=Performance/Fenix/Glossary)
     * for specific definitions.
     */
    enum class StartupPath {
        MAIN,
        VIEW,

        /**
         * The start up path if we received an Intent but we're unable to categorize it into other buckets.
         */
        UNKNOWN,

        /**
         * The start up path has not been set. This state includes:
         * - this API is accessed before it is set
         * - if no intent is received before the activity is STARTED (e.g. app switcher)
         */
        NOT_SET,
    }

    /**
     * Returns the [StartupPath] for the currently started activity. This value will be set
     * after an [Intent] is received that causes this activity to move into the STARTED state.
     */
    var startupPathForActivity = StartupPath.NOT_SET
        private set

    private var wasResumedSinceStartedState = false

    fun attachOnActivityOnCreate(lifecycle: Lifecycle, intent: Intent?) {
        lifecycle.addObserver(StartupPathLifecycleObserver())
        onIntentReceived(intent)
    }

    // N.B.: this method duplicates the actual logic for determining what page to open.
    // Unfortunately, it's difficult to re-use that logic because it occurs in many places throughout
    // the code so we do the simple thing for now and duplicate it. It's noticeably different from
    // what you might expect: e.g. ACTION_MAIN can open a URL and if ACTION_VIEW provides an invalid
    // URL, it'll perform a MAIN action. However, it's fairly representative of what users *intended*
    // to do when opening the app and shouldn't change much because it's based on Android system-wide
    // conventions, so it's probably fine for our purposes.
    private fun getStartupPathFromIntent(intent: Intent): StartupPath = when (intent.action) {
        Intent.ACTION_MAIN -> StartupPath.MAIN
        Intent.ACTION_VIEW -> StartupPath.VIEW
        else -> StartupPath.UNKNOWN
    }

    /**
     * Expected to be called when a new [Intent] is received by the [Activity]: i.e.
     * [Activity.onCreate] and [Activity.onNewIntent].
     */
    fun onIntentReceived(intent: Intent?) {
        // We want to set a path only if the intent causes the Activity to move into the STARTED state.
        // This means we want to discard any intents that are received when the app is foregrounded.
        // However, we can't use the Lifecycle.currentState to determine this because:
        // - the app is briefly paused (state becomes STARTED) before receiving the Intent in
        // the foreground so we can't say <= STARTED
        // - onIntentReceived can be called from the CREATED or STARTED state so we can't say == CREATED
        // So we're forced to track this state ourselves.
        if (!wasResumedSinceStartedState && intent != null) {
            startupPathForActivity = getStartupPathFromIntent(intent)
        }
    }

    @VisibleForTesting(otherwise = NONE)
    fun getTestCallbacks() = StartupPathLifecycleObserver()

    @VisibleForTesting(otherwise = PRIVATE)
    inner class StartupPathLifecycleObserver : DefaultLifecycleObserver {
        override fun onResume(owner: LifecycleOwner) {
            wasResumedSinceStartedState = true
        }

        override fun onStop(owner: LifecycleOwner) {
            // Clear existing state.
            wasResumedSinceStartedState = false
        }
    }
}
