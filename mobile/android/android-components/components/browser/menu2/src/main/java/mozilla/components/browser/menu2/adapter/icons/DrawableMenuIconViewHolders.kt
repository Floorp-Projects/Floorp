/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter.icons

import android.content.res.ColorStateList
import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageButton
import android.widget.ImageView
import androidx.annotation.LayoutRes
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet.BOTTOM
import androidx.constraintlayout.widget.ConstraintSet.END
import androidx.constraintlayout.widget.ConstraintSet.PARENT_ID
import androidx.constraintlayout.widget.ConstraintSet.START
import androidx.constraintlayout.widget.ConstraintSet.TOP
import androidx.core.view.isVisible
import kotlinx.coroutines.Job
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import mozilla.components.browser.menu2.R
import mozilla.components.browser.menu2.ext.applyIcon
import mozilla.components.browser.menu2.ext.applyNotificationEffect
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.AsyncDrawableMenuIcon
import mozilla.components.concept.menu.candidate.DrawableButtonMenuIcon
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.MenuIcon
import mozilla.components.support.base.log.logger.Logger

internal abstract class MenuIconWithDrawableViewHolder<T : MenuIcon>(
    parent: ConstraintLayout,
    inflater: LayoutInflater,
) : MenuIconViewHolder<T>(parent, inflater) {

    protected abstract val imageView: ImageView

    protected fun setup(imageView: View, side: Side) {
        updateConstraints {
            connect(R.id.icon, TOP, PARENT_ID, TOP)
            connect(R.id.icon, BOTTOM, PARENT_ID, BOTTOM)
            val margin = parent.resources
                .getDimensionPixelSize(R.dimen.mozac_browser_menu2_icon_padding_start)
            when (side) {
                Side.START -> {
                    connect(imageView.id, START, PARENT_ID, START)
                    connect(imageView.id, END, R.id.label, START, margin)
                    connect(R.id.label, START, imageView.id, END)
                }
                Side.END -> {
                    connect(imageView.id, END, PARENT_ID, END)
                    connect(imageView.id, START, R.id.label, END, margin)
                    connect(R.id.label, END, imageView.id, START)
                }
            }
        }
    }

    override fun disconnect() {
        parent.removeView(imageView)
        super.disconnect()
    }
}

internal class DrawableMenuIconViewHolder(
    parent: ConstraintLayout,
    inflater: LayoutInflater,
    side: Side,
) : MenuIconWithDrawableViewHolder<DrawableMenuIcon>(parent, inflater) {

    override val imageView: ImageView = inflate(layoutResource).findViewById(R.id.icon)
    private var effectView: ImageView? = null

    init {
        setup(imageView, side)
    }

    override fun bind(newIcon: DrawableMenuIcon, oldIcon: DrawableMenuIcon?) {
        imageView.applyIcon(newIcon, oldIcon)

        // Only inflate the effect container if needed
        if (newIcon.effect != null) {
            createEffectView().applyNotificationEffect(newIcon.effect as LowPriorityHighlightEffect, oldIcon?.effect)
        } else {
            effectView?.isVisible = false
        }
    }

    private fun createEffectView(): ImageView {
        if (effectView == null) {
            val effect: ImageView = inflate(notificationDotLayoutResource).findViewById(R.id.notification_dot)
            updateConstraints {
                connect(effect.id, TOP, imageView.id, TOP)
                connect(effect.id, END, imageView.id, END)
            }
            effectView = effect
        }
        return effectView!!
    }

    override fun disconnect() {
        effectView?.let { parent.removeView(it) }
        super.disconnect()
    }

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_icon_drawable
        val notificationDotLayoutResource = R.layout.mozac_browser_menu2_icon_notification_dot
    }
}

internal class DrawableButtonMenuIconViewHolder(
    parent: ConstraintLayout,
    inflater: LayoutInflater,
    side: Side,
    private val dismiss: () -> Unit,
) : MenuIconWithDrawableViewHolder<DrawableButtonMenuIcon>(parent, inflater), View.OnClickListener {

    override val imageView: ImageButton = inflate(layoutResource).findViewById(R.id.icon)
    private var onClickListener: (() -> Unit)? = null

    init {
        setup(imageView, side)
        imageView.setOnClickListener(this)
    }

    override fun bind(newIcon: DrawableButtonMenuIcon, oldIcon: DrawableButtonMenuIcon?) {
        imageView.applyIcon(newIcon, oldIcon)
        onClickListener = newIcon.onClick
    }

    override fun onClick(v: View?) {
        onClickListener?.invoke()
        dismiss()
    }

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_icon_button
    }
}

internal class AsyncDrawableMenuIconViewHolder(
    parent: ConstraintLayout,
    inflater: LayoutInflater,
    side: Side,
    private val logger: Logger = Logger("mozac-menu2-AsyncDrawableMenuIconViewHolder"),
) : MenuIconWithDrawableViewHolder<AsyncDrawableMenuIcon>(parent, inflater) {

    private val scope = MainScope()
    override val imageView: ImageView = inflate(layoutResource).findViewById(R.id.icon)
    private var effectView: ImageView? = null
    private var iconJob: Job? = null

    init {
        setup(imageView, side)
    }

    override fun bind(newIcon: AsyncDrawableMenuIcon, oldIcon: AsyncDrawableMenuIcon?) {
        if (newIcon.loadDrawable != oldIcon?.loadDrawable) {
            imageView.setImageDrawable(newIcon.loadingDrawable)
            iconJob?.cancel()
            iconJob = scope.launch { loadIcon(newIcon.loadDrawable, newIcon.fallbackDrawable) }
        }

        if (newIcon.tint != oldIcon?.tint) {
            imageView.imageTintList = newIcon.tint?.let { ColorStateList.valueOf(it) }
        }

        // Only inflate the effect container if needed
        if (newIcon.effect != null) {
            createEffectView().applyNotificationEffect(newIcon.effect as LowPriorityHighlightEffect, oldIcon?.effect)
        } else {
            effectView?.isVisible = false
        }
    }

    @Suppress("TooGenericExceptionCaught")
    private suspend fun loadIcon(
        loadDrawable: suspend (width: Int, height: Int) -> Drawable?,
        fallback: Drawable?,
    ) {
        val drawable = try {
            loadDrawable(imageView.measuredWidth, imageView.measuredHeight)
        } catch (throwable: Throwable) {
            logger.error(
                message = "Failed to load browser action icon, falling back to default.",
                throwable = throwable,
            )
            fallback
        }
        imageView.setImageDrawable(drawable)
    }

    private fun createEffectView(): ImageView {
        if (effectView == null) {
            val effect: ImageView = inflate(notificationDotLayoutResource).findViewById(R.id.notification_dot)
            updateConstraints {
                connect(effect.id, TOP, imageView.id, TOP)
                connect(effect.id, END, imageView.id, END)
            }
            effectView = effect
        }
        return effectView!!
    }

    override fun disconnect() {
        effectView?.let { parent.removeView(it) }
        scope.cancel()
        super.disconnect()
    }

    companion object {
        @LayoutRes
        val layoutResource = R.layout.mozac_browser_menu2_icon_drawable
        val notificationDotLayoutResource = R.layout.mozac_browser_menu2_icon_notification_dot
    }
}
