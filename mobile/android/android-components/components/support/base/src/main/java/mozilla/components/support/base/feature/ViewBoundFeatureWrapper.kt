/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

import android.content.Intent
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.LifecycleOwner

/**
 * Wrapper for [LifecycleAwareFeature] instances that keep a strong references to a [View]. This wrapper is helpful
 * when the lifetime of the [View] may be shorter than the [Lifecycle] and you need to keep a reference to the
 * [LifecycleAwareFeature] that may outlive the [View].
 *
 * [ViewBoundFeatureWrapper] takes care of stopping [LifecycleAwareFeature] and clearing references once the bound
 * [View] get detached.
 *
 * A common use case is a `Fragment` that needs to keep a reference to a [LifecycleAwareFeature] (e.g. to invoke
 * `onBackPressed()` and the [LifecycleAwareFeature] holds a reference to [View] instances. Once the `Fragment` gets
 * detached and not destroyed (e.g. when pushed to the back stack) it will still keep the reference to the
 * [LifecycleAwareFeature] and therefore to the (now detached) [View] (-> Leak). When the `Fragment` gets re-attached a
 * new [View] and matching [LifecycleAwareFeature] is getting created leading to multiple concurrent
 * [LifecycleAwareFeature] and (off-screen) [View] instances existing in memory.
 *
 * Example integration:
 *
 * ```
 * class MyFragment : Fragment {
 *     val myFeature = ViewBoundFeatureWrapper<MyFeature>()
 *
 *     override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
 *         // Bind wrapper to feature and view. Feature will be stopped and internal reference will be cleared
 *         // when the view gets detached.
 *         myFeature.set(
 *             feature = MyFeature(..., view),
 *             owner = this,
 *             view = view
 *         )
 *     }
 *
 *     fun doSomething() {
 *         // Get will return the feature instance or null if the instance was cleared (e.g. the fragment is detached)
 *         myFeature.get()?.doSomething()
 *     }
 *
 *     override fun onBackPressed(): Boolean {
 *         return myFeature.onBackPressed()
 *     }
 * }
 * ```
 */
class ViewBoundFeatureWrapper<T : LifecycleAwareFeature>() {
    private var feature: T? = null
    internal var owner: LifecycleOwner? = null
    private var view: View? = null

    private var viewBinding: ViewBinding<T>? = null
    private var lifecycleBinding: LifecycleBinding<T>? = null

    private var isFeatureStarted: Boolean = false

    /**
     * Convenient constructor for creating a wrapper instance and calling [set] immediately.
     */
    constructor(feature: T, owner: LifecycleOwner, view: View) : this() {
        set(feature, owner, view)
    }

    /**
     * Sets the [LifecycleAwareFeature] reference and binds it to the [Lifecycle] of the [LifecycleObserver] as well
     * as the [View].
     *
     * The wrapper will take care of subscribing to the [Lifecycle] and forwarding start/stop events to the
     * [LifecycleAwareFeature].
     *
     * Once the [View] gets detached the [LifecycleAwareFeature] will be stopped and the wrapper will clear all
     * internal references.
     */
    @Synchronized
    fun set(feature: T, owner: LifecycleOwner, view: View) {
        if (this.feature != null) {
            clear()
        }

        this.feature = feature
        this.owner = owner
        this.view = view

        viewBinding = ViewBinding(this).also {
            view.addOnAttachStateChangeListener(it)
        }

        lifecycleBinding = LifecycleBinding(this).also {
            owner.lifecycle.addObserver(it)
        }
    }

    /**
     * Returns the wrapped [LifecycleAwareFeature] or null if the [View] was detached and the reference was cleared.
     */
    @Synchronized
    fun get(): T? = feature

    /**
     * Runs the given [block] if this wrapper still has a reference to the [LifecycleAwareFeature].
     */
    @Synchronized
    fun withFeature(block: (T) -> Unit) {
        feature?.let(block)
    }

    /**
     * Stops the feature and clears all internal references and observers.
     */
    @Synchronized
    fun clear() {
        // Stop feature and clear reference
        if (isFeatureStarted) {
            feature?.stop()
        }
        feature = null

        // Stop observing view and clear references
        view?.removeOnAttachStateChangeListener(viewBinding)
        view = null
        viewBinding = null

        // Stop observing lifecycle and clear references
        lifecycleBinding?.let {
            owner?.lifecycle?.removeObserver(it)
        }
        owner = null
        lifecycleBinding = null
    }

    /**
     * Convenience method for invoking [UserInteractionHandler.onBackPressed] on a wrapped
     * [LifecycleAwareFeature] that implements [UserInteractionHandler]. Returns false if
     * the [LifecycleAwareFeature] was cleared already.
     */
    @Synchronized
    fun onBackPressed(): Boolean {
        val feature = feature ?: return false

        if (feature !is UserInteractionHandler) {
            throw IllegalAccessError(
                "Feature does not implement ${UserInteractionHandler::class.java.simpleName} interface",
            )
        }

        return feature.onBackPressed()
    }

    /**
     * Convenience method for invoking [ActivityResultHandler.onActivityResult] on a wrapped
     * [LifecycleAwareFeature] that implements [ActivityResultHandler]. Returns false if
     * the [LifecycleAwareFeature] was cleared already.
     */
    @Synchronized
    fun onActivityResult(requestCode: Int, data: Intent?, resultCode: Int): Boolean {
        val feature = feature ?: return false

        if (feature !is ActivityResultHandler) {
            throw IllegalAccessError(
                "Feature does not implement ${ActivityResultHandler::class.java.simpleName} interface",
            )
        }

        return feature.onActivityResult(requestCode, data, resultCode)
    }

    @Synchronized
    internal fun start() {
        feature?.start()
        isFeatureStarted = true
    }

    @Synchronized
    internal fun stop() {
        feature?.stop()
        isFeatureStarted = false
    }
}

/**
 * [View.OnAttachStateChangeListener] implementation to call [ViewBoundFeatureWrapper.clear] in case the [View] gets
 * detached.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal class ViewBinding<T : LifecycleAwareFeature>(
    private val wrapper: ViewBoundFeatureWrapper<T>,
) : View.OnAttachStateChangeListener {
    override fun onViewDetachedFromWindow(v: View?) {
        wrapper.clear()
    }

    override fun onViewAttachedToWindow(v: View?) = Unit
}

/**
 * [LifecycleObserver] implementation to forward start/stop events to the [ViewBoundFeatureWrapper]. Additionally
 * this implementation will call [ViewBoundFeatureWrapper.clear] in case the [Lifecycle] gets destroyed.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal class LifecycleBinding<T : LifecycleAwareFeature>(
    private val wrapper: ViewBoundFeatureWrapper<T>,
) : DefaultLifecycleObserver {
    override fun onStart(owner: LifecycleOwner) {
        wrapper.start()
    }

    override fun onStop(owner: LifecycleOwner) {
        wrapper.stop()
    }

    override fun onDestroy(owner: LifecycleOwner) {
        wrapper.clear()
    }
}
