/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.support.annotation.VisibleForTesting
import android.support.v4.app.FragmentManager
import android.view.HapticFeedbackConstants
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.contextmenu.facts.emitClickFact
import mozilla.components.support.base.feature.LifecycleAwareFeature

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal const val FRAGMENT_TAG = "mozac_feature_contextmenu_dialog"

/**
 * Feature for displaying a context menu after long-pressing web content.
 *
 * This feature will subscribe to the currently selected [Session] and display the context menu based on
 * [Session.Observer.onLongPress] events. Once the context menu is closed or the user selects an item from the context
 * menu the related [HitResult] will be consumed.
 *
 * @property fragmentManager The [FragmentManager] to be used when displaying a context menu (fragment).
 * @property sessionManager The [SessionManager] instance in order to subscribe to the selected [Session].
 * @property candidates A list of [ContextMenuCandidate] objects. For every observed [HitResult] this feature will query
 * all candidates ([ContextMenuCandidate.showFor]) in order to determine which candidates want to show up in the context
 * menu. If a context menu item was selected by the user the feature will invoke the [ContextMenuCandidate.action]
 * method of the related candidate.
 * @property engineView The [EngineView]] this feature component should show context menus for.
 */
class ContextMenuFeature(
    private val fragmentManager: FragmentManager,
    private val sessionManager: SessionManager,
    private val candidates: List<ContextMenuCandidate>,
    private val engineView: EngineView,
    private val sessionId: String? = null
) : LifecycleAwareFeature {
    private val observer = ContextMenuObserver(sessionManager, feature = this)

    /**
     * Start observing the selected session and when needed show a context menu.
     */
    override fun start() {
        fragmentManager.findFragmentByTag(FRAGMENT_TAG)?.let { fragment ->
            // There's still a context menu fragment visible from the last time. Re-attach this feature so that the
            // fragment can invoke the callback on this feature once the user makes a selection. This can happen when
            // the app was in the background and on resume the activity and fragments get recreated.
            reattachFragment(fragment as ContextMenuFragment)
        }

        observer.start(sessionId)
    }

    /**
     * Stop observing the selected session and do not show any context menus anymore.
     */
    override fun stop() {
        observer.stop()
    }

    /**
     * Re-attach a fragment that is still visible but not linked to this feature anymore.
     */
    private fun reattachFragment(fragment: ContextMenuFragment) {
        val session = sessionManager.findSessionById(fragment.sessionId)

        if (session == null || session.hitResult.isConsumed()) {
            // If the session no longer exists or if it has no hit result (from long pressing) attached anymore then
            // there's no reason to still show the context menu. Let's remove the fragment.
            fragmentManager.beginTransaction()
                .remove(fragment)
                .commitAllowingStateLoss()
            return
        }

        // Re-assign the feature instance so that the fragment can invoke us once the user makes a selection or cancels
        // the dialog.
        fragment.feature = this
    }

    internal fun onLongPress(session: Session, hitResult: HitResult) {
        val (ids, labels) = candidates
            .filter { candidate -> candidate.showFor(session, hitResult) }
            .fold(Pair(mutableListOf<String>(), mutableListOf<String>())) { items, candidate ->
                items.first.add(candidate.id)
                items.second.add(candidate.label)
                items
            }

        // We have no context menu items to show for this HitResult. Let's consume it to remove it from the Session.
        if (ids.isEmpty()) {
            session.hitResult.consume { true }
            return
        }

        // We know that we are going to show a context menu. Now is the time to perform the haptic feedback.
        engineView.asView().performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)

        val fragment = ContextMenuFragment.create(session, hitResult.src, ids, labels)
        fragment.feature = this
        fragment.show(fragmentManager, FRAGMENT_TAG)
    }

    internal fun onMenuItemSelected(sessionId: String, itemId: String) {
        val session = sessionManager.findSessionById(sessionId) ?: return
        val candidate = candidates.find { it.id == itemId } ?: return

        session.hitResult.consume { hitResult ->
            candidate.action.invoke(session, hitResult)
            emitClickFact(candidate)
            true
        }
    }

    internal fun onMenuCancelled(sessionId: String) {
        val session = sessionManager.findSessionById(sessionId) ?: return
        session.hitResult.consume { true }
    }
}

/**
 * Observes [Session.Observer.onLongPress] of the selected session and notifies the feature whenever a context menu
 * needs to be shown.
 */
internal class ContextMenuObserver(
    sessionManager: SessionManager,
    private val feature: ContextMenuFeature
) : SelectionAwareSessionObserver(sessionManager) {
    override fun onLongPress(session: Session, hitResult: HitResult): Boolean {
        feature.onLongPress(session, hitResult)
        return false
    }

    fun start(sessionId: String?) {
        observeIdOrSelected(sessionId)
    }
}
