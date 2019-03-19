package org.mozilla.fenix.quickactionsheet

import android.content.Context
import android.util.AttributeSet
import android.util.Log
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.widget.NestedScrollView
import com.google.android.material.bottomsheet.BottomSheetBehavior
import kotlinx.android.synthetic.main.component_quickactionsheet.view.*
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.findinpage.view.FindInPageBar
import org.mozilla.fenix.R
import kotlin.math.absoluteValue

class QuickActionSheet @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
    defStyleRes: Int = 0
) : LinearLayout(context, attrs, defStyle, defStyleRes) {

    var currentMargin : Float = 0f
    var previousY : Float = 0f

    init {
        LayoutInflater.from(context)
            .inflate(R.layout.component_quickactionsheet, this, true)
    }

    /*
    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        val linearLayout = findViewById<LinearLayout>(R.id.quick_action_sheet)
        val bottomSheetBehavior = BottomSheetBehavior.from(linearLayout)
        bottomSheetBehavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

*/

    /*

    override fun onInterceptTouchEvent(ev: MotionEvent?): Boolean {
        Log.d("touchEvent", "starting: " + rootView.y)
        return true
    }

    override fun onTouchEvent(event: MotionEvent?): Boolean {
        /*
        val layout = findViewById<LinearLayout>(R.id.quick_action_sheet)

        val params = layout.layoutParams as ViewGroup.MarginLayoutParams

        when (event?.action) {
            MotionEvent.ACTION_MOVE -> {
                Log.d("touchEvent", "handle move: " + (currentMargin))


                previousY = event.y

                params.bottomMargin += diff.toInt()
                requestLayout()
            }
            MotionEvent.ACTION_UP -> Log.d("touchEvent", "handle up")
            MotionEvent.ACTION_DOWN -> Log.d("touchEvent", "handle down")
            else -> Log.d("touchEvent", "" + event?.action)
        }
        */
        return true
    }
    */
}

@Suppress("unused") // Referenced from XML
class QuickActionSheetBehavior(
    context: Context,
    attrs: AttributeSet
) : BottomSheetBehavior<NestedScrollView>(context, attrs) {
    override fun layoutDependsOn(parent: CoordinatorLayout, child: NestedScrollView, dependency: View): Boolean {
        if (dependency is BrowserToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onDependentViewChanged(
        parent: CoordinatorLayout,
        child: NestedScrollView,
        dependency: View
    ): Boolean {
        return if (dependency is BrowserToolbar) {
            repositionQuickActionSheet(child, dependency)
            true
        } else {
            false
        }
    }

    private fun repositionQuickActionSheet(quickActionSheetContainer: NestedScrollView, toolbar: BrowserToolbar) {
        state = BottomSheetBehavior.STATE_COLLAPSED
        quickActionSheetContainer.translationY = (toolbar.translationY + toolbar.height * -1.0).toFloat()
    }
}