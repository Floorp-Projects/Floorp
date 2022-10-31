/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Color
import android.util.AttributeSet
import android.widget.FrameLayout
import androidx.annotation.VisibleForTesting
import androidx.core.view.ViewCompat
import mozilla.components.browser.engine.gecko.selection.GeckoSelectionActionDelegate
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.mediaquery.PreferredColorScheme
import mozilla.components.concept.engine.selection.SelectionActionDelegate
import org.mozilla.geckoview.BasicSelectionActionDelegate
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession

/**
 * Gecko-based EngineView implementation.
 */
class GeckoEngineView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : FrameLayout(context, attrs, defStyleAttr), EngineView {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var geckoView = object : NestedGeckoView(context) {

        override fun onAttachedToWindow() {
            try {
                super.onAttachedToWindow()
            } catch (e: IllegalStateException) {
                // This is to debug "display already acquired" crashes
                val otherActivityClassName =
                    this.session?.accessibility?.view?.context?.javaClass?.simpleName
                val otherActivityClassHashcode =
                    this.session?.accessibility?.view?.context?.hashCode()
                val activityClassName = context.javaClass.simpleName
                val activityClassHashCode = context.hashCode()
                val msg = "ATTACH VIEW: Current activity: $activityClassName hashcode " +
                    "$activityClassHashCode Other activity: $otherActivityClassName " +
                    "hashcode $otherActivityClassHashcode"
                throw IllegalStateException(msg, e)
            }
        }

        override fun onDetachedFromWindow() {
            // We are releasing the session before GeckoView gets detached from the window. Otherwise
            // GeckoView will close the session automatically and we do not want that.
            releaseSession()

            super.onDetachedFromWindow()
        }
    }.apply {
        // Explicitly mark this view as important for autofill. The default "auto" doesn't seem to trigger any
        // autofill behavior for us here.
        @Suppress("WrongConstant")
        ViewCompat.setImportantForAutofill(this, ViewCompat.IMPORTANT_FOR_ACCESSIBILITY_YES)
    }

    internal fun setColorScheme(preferredColorScheme: PreferredColorScheme) {
        var colorScheme = preferredColorScheme
        if (preferredColorScheme == PreferredColorScheme.System) {
            colorScheme =
                if (context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK
                    == Configuration.UI_MODE_NIGHT_YES
                ) {
                    PreferredColorScheme.Dark
                } else {
                    PreferredColorScheme.Light
                }
        }

        if (colorScheme == PreferredColorScheme.Dark) {
            geckoView.coverUntilFirstPaint(DARK_COVER)
        } else {
            geckoView.coverUntilFirstPaint(Color.WHITE)
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var currentSession: GeckoEngineSession? = null

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var currentSelection: BasicSelectionActionDelegate? = null

    override var selectionActionDelegate: SelectionActionDelegate? = null

    init {
        addView(geckoView)
    }

    /**
     * Render the content of the given session.
     */
    @Synchronized
    override fun render(session: EngineSession) {
        val internalSession = session as GeckoEngineSession
        currentSession = session

        if (geckoView.session != internalSession.geckoSession) {
            geckoView.session?.let {
                // Release a previously assigned session. Otherwise GeckoView will close it
                // automatically.
                detachSelectionActionDelegate(it)
                geckoView.releaseSession()
            }

            try {
                geckoView.setSession(internalSession.geckoSession)
                attachSelectionActionDelegate(internalSession.geckoSession)
            } catch (e: IllegalStateException) {
                // This is to debug "display already acquired" crashes
                val otherActivityClassName =
                    internalSession.geckoSession.accessibility.view?.context?.javaClass?.simpleName
                val otherActivityClassHashcode =
                    internalSession.geckoSession.accessibility.view?.context?.hashCode()
                val activityClassName = context.javaClass.simpleName
                val activityClassHashCode = context.hashCode()
                val msg = "SET SESSION: Current activity: $activityClassName hashcode " +
                    "$activityClassHashCode Other activity: $otherActivityClassName " +
                    "hashcode $otherActivityClassHashcode"
                throw IllegalStateException(msg, e)
            }
        }
    }

    private fun attachSelectionActionDelegate(session: GeckoSession) {
        val delegate = GeckoSelectionActionDelegate.maybeCreate(context, selectionActionDelegate)
        if (delegate != null) {
            session.selectionActionDelegate = delegate
            currentSelection = delegate
        }
    }

    private fun detachSelectionActionDelegate(session: GeckoSession?) {
        if (currentSelection != null) {
            session?.selectionActionDelegate = null
            currentSelection = null
        }
    }

    @Synchronized
    override fun release() {
        detachSelectionActionDelegate(currentSession?.geckoSession)

        currentSession = null

        geckoView.releaseSession()
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        release()
    }

    override fun canClearSelection() = !currentSelection?.selection?.text.isNullOrEmpty()

    override fun canScrollVerticallyUp() = currentSession?.let { it.scrollY > 0 } != false

    override fun canScrollVerticallyDown() =
        true // waiting for this issue https://bugzilla.mozilla.org/show_bug.cgi?id=1507569

    override fun getInputResultDetail() = geckoView.inputResultDetail

    override fun setVerticalClipping(clippingHeight: Int) {
        geckoView.setVerticalClipping(clippingHeight)
    }

    override fun setDynamicToolbarMaxHeight(height: Int) {
        geckoView.setDynamicToolbarMaxHeight(height)
    }

    @Suppress("TooGenericExceptionCaught")
    override fun captureThumbnail(onFinish: (Bitmap?) -> Unit) {
        try {
            val geckoResult = geckoView.capturePixels()
            geckoResult.then(
                { bitmap ->
                    onFinish(bitmap)
                    GeckoResult<Void>()
                },
                {
                    onFinish(null)
                    GeckoResult<Void>()
                },
            )
        } catch (e: Exception) {
            // There's currently no reliable way for consumers of GeckoView to
            // know whether or not the compositor is ready. So we have to add
            // a catch-all here. In the future, GeckoView will invoke our error
            // callback instead and this block can be removed:
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1645114
            // https://github.com/mozilla-mobile/android-components/issues/6680
            onFinish(null)
        }
    }

    override fun clearSelection() {
        currentSelection?.clearSelection()
    }

    override fun setVisibility(visibility: Int) {
        // GeckoView doesn't react to onVisibilityChanged so we need to propagate ourselves for now:
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1630775
        // We do this to prevent the content from resizing when the view is not visible:
        // https://github.com/mozilla-mobile/android-components/issues/6664
        geckoView.visibility = visibility
        super.setVisibility(visibility)
    }

    companion object {
        internal const val DARK_COVER = 0xFF2A2A2E.toInt()
    }
}
