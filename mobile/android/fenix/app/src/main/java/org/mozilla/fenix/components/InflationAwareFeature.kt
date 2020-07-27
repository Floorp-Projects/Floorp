/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import android.view.ViewStub
import androidx.annotation.UiThread
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import java.lang.ref.WeakReference

/**
 * A base feature class that enables lazy inflation of a view needed by a feature.
 *
 * When a feature needs to be launched (e.g. by user interaction) calling [launch]
 * will inflate the view only then, start the feature, and then executes [onLaunch]
 * for any feature-specific startup needs.
 */
abstract class InflationAwareFeature(
    private val stub: ViewStub
) : LifecycleAwareFeature, UserInteractionHandler {

    internal lateinit var view: WeakReference<View>
    internal var feature: LifecycleAwareFeature? = null
    private val stubListener = ViewStub.OnInflateListener { _, inflated ->
        view = WeakReference(inflated)
        feature = onViewInflated(inflated).also {
            it.start()
            onLaunch(inflated, it)
        }
    }

    /**
     * Invoked when a view-dependent feature needs to be started along with the feature itself.
     */
    @UiThread
    fun launch() {
        // If we have a feature and view, we can launch immediately.
        if (feature != null && view.get() != null) {
            onLaunch(view.get()!!, feature!!)
        } else {
            stub.apply {
                setOnInflateListener(stubListener)
                inflate()
            }
        }
    }

    /**
     * Implementation notes: This implemented method does nothing since we only start the feature
     * when the view is inflated.
     */
    override fun start() {
        feature?.start()
    }

    override fun stop() {
        feature?.stop()
    }

    /**
     * Called when the feature gets the option to handle the user pressing the back key.
     *
     * @return true if the feature also implements [UserInteractionHandler] and the feature has
     * been initiated.
     */
    override fun onBackPressed(): Boolean {
        return (feature as? UserInteractionHandler)?.onBackPressed() ?: false
    }

    /**
     * Invoked when the view has been inflated for the feature to be created with it.
     *
     * @param view The newly created view.
     * @return The feature initiated with the view.
     */
    abstract fun onViewInflated(view: View): LifecycleAwareFeature

    /**
     * Invoked after the feature is instantiated. If the feature already exists,
     * this is invoked immediately.
     *
     * @param view The view that is attached to the feature.
     * @param feature The feature that was instantiated.
     */
    abstract fun onLaunch(view: View, feature: LifecycleAwareFeature)
}
