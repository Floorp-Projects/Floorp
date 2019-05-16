/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.progress

import android.content.res.Resources
import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.PorterDuff
import android.graphics.Rect
import android.graphics.Region
import android.graphics.drawable.Drawable
import androidx.annotation.IntRange
import android.util.AttributeSet

import org.xmlpull.v1.XmlPullParser
import org.xmlpull.v1.XmlPullParserException

import java.io.IOException

/**
 * DrawableWrapper was added since API Level 23. But in v7 support library, it has annotation
 * "@RestrictTo(LIBRARY_GROUP)". Hence we should not extends it, so we create this wrapper for now.
 * Once we start to support API 23, or v7-support-library allows us to extends its DrawableWrapper,
 * then this file can be removed.
 */
@Suppress("TooManyFunctions") // We are overriding functions of Drawable
internal open class DrawableWrapper(val wrappedDrawable: Drawable) : Drawable() {

    override fun draw(canvas: Canvas) {
        wrappedDrawable.draw(canvas)
    }

    override fun getChangingConfigurations(): Int {
        return wrappedDrawable.changingConfigurations
    }

    override fun getConstantState(): ConstantState? {
        return wrappedDrawable.constantState
    }

    override fun getCurrent(): Drawable {
        return wrappedDrawable.current
    }

    override fun getIntrinsicHeight(): Int {
        return wrappedDrawable.intrinsicHeight
    }

    override fun getIntrinsicWidth(): Int {
        return wrappedDrawable.intrinsicWidth
    }

    override fun getMinimumHeight(): Int {
        return wrappedDrawable.minimumHeight
    }

    override fun getMinimumWidth(): Int {
        return wrappedDrawable.minimumWidth
    }

    override fun getOpacity(): Int {
        return wrappedDrawable.opacity
    }

    override fun getPadding(padding: Rect): Boolean {
        return wrappedDrawable.getPadding(padding)
    }

    override fun getState(): IntArray {
        return wrappedDrawable.state
    }

    override fun getTransparentRegion(): Region? {
        return wrappedDrawable.transparentRegion
    }

    @Throws(XmlPullParserException::class, IOException::class)
    override fun inflate(r: Resources, parser: XmlPullParser, attrs: AttributeSet) {
        wrappedDrawable.inflate(r, parser, attrs)
    }

    override fun isStateful(): Boolean {
        return wrappedDrawable.isStateful
    }

    override fun jumpToCurrentState() {
        wrappedDrawable.jumpToCurrentState()
    }

    override fun mutate(): Drawable {
        return wrappedDrawable.mutate()
    }

    override fun setAlpha(@IntRange(from = MIN_ALPHA_VALUE, to = MAX_ALPHA_VALUE) i: Int) {
        wrappedDrawable.alpha = i
    }

    override fun scheduleSelf(what: Runnable, `when`: Long) {
        wrappedDrawable.scheduleSelf(what, `when`)
    }

    override fun setChangingConfigurations(configs: Int) {
        wrappedDrawable.changingConfigurations = configs
    }

    override fun setColorFilter(colorFilter: ColorFilter?) {
        wrappedDrawable.colorFilter = colorFilter
    }

    override fun setColorFilter(color: Int, mode: PorterDuff.Mode) {
        wrappedDrawable.setColorFilter(color, mode)
    }

    override fun setFilterBitmap(filter: Boolean) {
        wrappedDrawable.isFilterBitmap = filter
    }

    override fun setVisible(visible: Boolean, restart: Boolean): Boolean {
        return wrappedDrawable.setVisible(visible, restart)
    }

    override fun unscheduleSelf(what: Runnable) {
        wrappedDrawable.unscheduleSelf(what)
    }

    override fun onBoundsChange(bounds: Rect) {
        wrappedDrawable.bounds = bounds
    }

    override fun onLevelChange(level: Int): Boolean {
        return wrappedDrawable.setLevel(level)
    }

    override fun onStateChange(state: IntArray): Boolean {
        return wrappedDrawable.setState(state)
    }
}

private const val MIN_ALPHA_VALUE = 0L
private const val MAX_ALPHA_VALUE = 255L
