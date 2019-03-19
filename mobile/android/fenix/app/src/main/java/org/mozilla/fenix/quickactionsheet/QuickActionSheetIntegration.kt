package org.mozilla.fenix.quickactionsheet

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.findinpage.view.FindInPageBar

/**
 * [CoordinatorLayout.Behavior] that will always position the [QuickActionSheetBar] above the [BrowserToolbar] (including
 * when the browser toolbar is scrolling or performing a snap animation).
 */
@Suppress("unused") // Referenced from XML
class QuickActionSheetIntegration(
    context: Context,
    attrs: AttributeSet
) : CoordinatorLayout.Behavior<FindInPageBar>(context, attrs) {
    override fun layoutDependsOn(parent: CoordinatorLayout, child: FindInPageBar, dependency: View): Boolean {
        if (dependency is BrowserToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(parent: CoordinatorLayout, child: FindInPageBar, dependency: View): Boolean {
        return if (dependency is BrowserToolbar) {
            repositionFindInPageBar(child, dependency)
            true
        } else {
            false
        }
    }

    private fun repositionFindInPageBar(findInPageView: FindInPageBar, toolbar: BrowserToolbar) {
        findInPageView.translationY = (toolbar.translationY + toolbar.height * -1.0).toFloat()
    }
}