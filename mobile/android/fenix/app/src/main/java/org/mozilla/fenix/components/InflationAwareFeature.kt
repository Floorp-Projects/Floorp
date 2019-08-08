package org.mozilla.fenix.components

import android.view.View
import android.view.ViewStub
import androidx.annotation.UiThread
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
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
) : LifecycleAwareFeature, BackHandler {

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
        // We don't do anything because we only want to start the feature when it's being used.
    }

    override fun stop() {
        feature?.stop()
    }

    /**
     * Called when the feature gets the option to handle the user pressing the back key.
     *
     * @return true if the feature also implements [BackHandler] and the feature has been initiated.
     */
    override fun onBackPressed(): Boolean {
        return (feature as? BackHandler)?.onBackPressed() ?: false
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
