/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.compose

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.PixelFormat
import android.view.View
import android.view.WindowManager
import androidx.annotation.Px
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.AbstractComposeView
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.ViewRootForInspector
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntRect
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Popup
import androidx.compose.ui.window.PopupPositionProvider
import androidx.compose.ui.window.PopupProperties
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.lifecycle.ViewTreeLifecycleOwner
import androidx.savedstate.ViewTreeSavedStateRegistryOwner
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.focus.R
import org.mozilla.focus.R.color
import org.mozilla.focus.ui.theme.FocusTheme
import org.mozilla.gecko.GeckoScreenOrientation
import kotlin.math.absoluteValue
import kotlin.math.roundToInt

/**
 * Fixed width for all CFRs.
 */
private const val POPUP_WIDTH_DP = 256

/**
 * Fixed horizontal padding.
 * Allows the close button to extend with 10dp more to the end and intercept touches to a bit outside of the popup
 * to ensure it respects a11y recomendations of 48dp size while also offer a bit more space to the text.
 */
private const val HORIZONTAL_PADDING = 10

/**
 * Value class allowing to easily reason about what an `Int` represents.
 * This is compiled to the underlying `Int` type so incurs no performance penalty.
 */
@JvmInline
internal value class Pixels(val value: Int)

/**
 * Simple wrapper over the absolute x-coordinates of the popup. Includes any paddings.
 */
internal data class PopupHorizontalBounds(
    val startCoord: Pixels,
    val endCoord: Pixels
)

/**
 * Properties used to customize the behavior of a [CFRPopup].
 *
 * @property dismissOnBackPress Whether the popup can be dismissed by pressing the back button.
 * If true, pressing the back button will also call onDismiss().
 * @property dismissOnClickOutside Whether the popup can be dismissed by clicking outside the
 * popup's bounds. If true, clicking outside the popup will call onDismiss().
 * @property overlapAnchor How the popup will be anchored with it's top-start corner:
 *   - true - popup will be anchored in the exactly in the middle horizontally and vertically
 *   - false - popup will be anchored horizontally in the middle of the anchor but immediately below it
 * @property indicatorArrowStartOffset Maximum distance between the popup start and the indicator arrow.
 * If there isn't enough space this could automatically be overridden up to 0.
 * @property indicatorArrowHeight How tall the indicator arrow should be.
 * This will also affect how wide the base of the indicator arrow will be.
 * @property elevation The z-coordinate at which to place the popup.
 * This controls the size of the shadow below the popup.
 */
data class CFRPopupProperties(
    val dismissOnBackPress: Boolean = true,
    val dismissOnClickOutside: Boolean = true,
    val overlapAnchor: Boolean = false,
    val indicatorArrowStartOffset: Dp = 30.dp,
    val indicatorArrowHeight: Dp = 10.dp,
    val elevation: Dp = 10.dp
)

/**
 * CFR - Contextual Feature Recommendation popup.
 *
 * @param container [View] that this popup's lifecycle will be tied to.
 * @param text [String] shown as the popup content.
 * @param anchor [View] that will serve as the anchor of the popup.
 * @param onDismiss Callback for when the popup is dismissed.
 * @param properties [CFRPopupProperties] allowing to customize the popup behavior.
 */
class CFRPopup(
    private val container: View,
    private val text: String,
    private val anchor: View,
    private val properties: CFRPopupProperties = CFRPopupProperties(),
    private val onDismiss: () -> Unit = {}
) {
    // This is just a facade for CFRPopupFullScreenLayout to offer a cleaner API.

    /**
     * Construct and display a styled CFR popup shown at the coordinates of [anchor].
     * This popup will be dismissed when the user clicks on the "x" button or based on other user actions
     * with such behavior set in [CFRPopupProperties].
     */
    fun show() {
        anchor.post {
            CFRPopupFullScreenLayout(container, anchor, text, properties, onDismiss).apply {
                this.show()
            }
        }
    }
}

/**
 * [AbstractComposeView] that can be added or removed dynamically in the current window to display
 * a [Composable] based popup anywhere on the screen.
 */
@OptIn(ExperimentalComposeUiApi::class)
@SuppressLint("ViewConstructor") // Intended to be used only in code, don't need a View constructor
@VisibleForTesting
internal class CFRPopupFullScreenLayout(
    private val container: View,
    private val anchor: View,
    private val text: String,
    private val properties: CFRPopupProperties,
    private val onDismiss: () -> Unit
) : AbstractComposeView(container.context), ViewRootForInspector {
    private val windowManager =
        container.context.getSystemService(Context.WINDOW_SERVICE) as WindowManager

    /**
     * Listener for when the anchor is removed from the screen.
     * Useful in the following situations:
     *   - lack of purpose - if there is no anchor the context/action to which this popup refers to disappeared
     *   - leak from WindowManager - if removing the app from task manager while the popup is shown.
     *
     * Will not inform client about this since the user did not expressly dismissed this popup.
     */
    private val anchorDetachedListener = OnViewDetachedListener {
        dismiss()
    }

    /**
     * When the screen is rotated the popup may get improperly anchored
     * because of the async nature of insets and screen rotation.
     * To avoid any improper anchorage the popups are automatically dismissed.
     * Will not inform client about this since the user did not expressly dismissed this popup.
     */
    private val orientationChangeListener = GeckoScreenOrientation.OrientationChangeListener {
        dismiss()
    }

    override var shouldCreateCompositionOnAttachedToWindow: Boolean = false
        private set

    init {
        ViewTreeLifecycleOwner.set(this, ViewTreeLifecycleOwner.get(container))
        ViewTreeSavedStateRegistryOwner.set(this, ViewTreeSavedStateRegistryOwner.get(container))
        GeckoScreenOrientation.getInstance().addListener(orientationChangeListener)
        anchor.addOnAttachStateChangeListener(anchorDetachedListener)
    }

    /**
     * Add a new CFR popup to the current window overlaying everything already displayed.
     * This popup will be dismissed when the user clicks on the "x" button or based on other user actions
     * with such behavior set in [CFRPopupProperties].
     */
    fun show() {
        windowManager.addView(this, createLayoutParams())
    }

    @Composable
    override fun Content() {
        val anchorLocation = IntArray(2).apply {
            anchor.getLocationOnScreen(this)
        }

        val anchorXCoordMiddle = Pixels(anchorLocation.first() + anchor.width / 2)
        val indicatorArrowHeight = Pixels(properties.indicatorArrowHeight.toPx())

        val popupBounds = computePopupHorizontalBounds(
            anchorMiddleXCoord = anchorXCoordMiddle,
            arrowIndicatorWidth = Pixels(CFRPopupShape.getIndicatorBaseWidthForHeight(indicatorArrowHeight.value)),
        )
        val indicatorOffset = computeIndicatorArrowStartCoord(
            anchorMiddleXCoord = anchorXCoordMiddle,
            popupStartCoord = popupBounds.startCoord,
            arrowIndicatorWidth = Pixels(
                CFRPopupShape.getIndicatorBaseWidthForHeight(
                    properties.indicatorArrowHeight.toPx()
                )
            )
        )

        Popup(
            popupPositionProvider = object : PopupPositionProvider {
                override fun calculatePosition(
                    anchorBounds: IntRect,
                    windowSize: IntSize,
                    layoutDirection: LayoutDirection,
                    popupContentSize: IntSize
                ): IntOffset {
                    // Popup will be anchored such that the indicator arrow will point to the middle of the anchor View
                    // but the popup is allowed some space as start padding in which it can be displayed such that
                    // the indicator arrow is now exactly at the top-start corner but slightly translated to end.
                    // Values are in pixels.
                    return IntOffset(
                        when (layoutDirection) {
                            LayoutDirection.Ltr -> popupBounds.startCoord.value
                            else -> popupBounds.endCoord.value
                        },
                        when (properties.overlapAnchor) {
                            true -> anchorLocation.last() + anchor.height / 2
                            else -> anchorLocation.last() + anchor.height
                        }
                    )
                }
            },
            properties = PopupProperties(
                focusable = properties.dismissOnBackPress,
                dismissOnBackPress = properties.dismissOnBackPress,
                dismissOnClickOutside = properties.dismissOnClickOutside,
            ),
            onDismissRequest = {
                // For when tapping outside the popup or on the back button.
                dismiss()
                onDismiss()
            }
        ) {
            CFRPopupContent(
                text = text,
                indicatorArrowStartOffset = with(LocalDensity.current) {
                    (indicatorOffset.value).toDp()
                },
                indicatorArrowHeight = properties.indicatorArrowHeight,
                elevation = properties.elevation,
                onDismissButton = {
                    // For when tapping the "X" button.
                    dismiss()
                    onDismiss()
                }
            )
        }
    }

    /**
     * Compute the x-coordinates for the absolute start and end position of the popup, including any padding.
     * This assumes anchoring is indicated with an arrow to the horizontal middle of the anchor with the popup's
     * body potentially extending to the `start` of the arrow indicator.
     *
     * @param anchorMiddleXCoord x-coordinate for the middle of the anchor.
     * @param arrowIndicatorWidth x-distance the arrow indicator occupies.
     */
    @Composable
    @VisibleForTesting
    internal fun computePopupHorizontalBounds(
        anchorMiddleXCoord: Pixels,
        arrowIndicatorWidth: Pixels
    ): PopupHorizontalBounds {
        val arrowIndicatorHalfWidth = arrowIndicatorWidth.value / 2

        if (LocalConfiguration.current.layoutDirection == View.LAYOUT_DIRECTION_LTR) {
            // Push the popup as far to the start as needed including any needed paddings.
            val startCoord = Pixels(
                (anchorMiddleXCoord.value - arrowIndicatorHalfWidth)
                    .minus(properties.indicatorArrowStartOffset.toPx())
                    .minus(HORIZONTAL_PADDING.dp.toPx())
                    .coerceAtLeast(getLeftInsets())
            )

            return PopupHorizontalBounds(
                startCoord = startCoord,
                endCoord = Pixels(
                    startCoord.value + POPUP_WIDTH_DP.dp.toPx() + HORIZONTAL_PADDING.dp.toPx() * 2
                )
            )
        } else {
            val startCoord = Pixels(
                // Push the popup as far to the start (in RTL) as possible.
                anchorMiddleXCoord.value
                    .plus(arrowIndicatorHalfWidth)
                    .plus(properties.indicatorArrowStartOffset.toPx())
                    .plus(HORIZONTAL_PADDING.dp.toPx())
                    .coerceAtMost(
                        LocalDensity.current.run {
                            LocalConfiguration.current.screenWidthDp.dp.toPx()
                        }
                            .roundToInt()
                            .plus(getLeftInsets())
                    )
            )
            return PopupHorizontalBounds(
                startCoord = startCoord,
                endCoord = Pixels(
                    startCoord.value - POPUP_WIDTH_DP.dp.toPx() - HORIZONTAL_PADDING.dp.toPx() * 2
                )
            )
        }
    }

    /**
     * Compute the x-coordinate for where the popup's indicator arrow should start
     * relative to the available distance between it and the popup's starting x-coordinate.
     *
     * @param anchorMiddleXCoord x-coordinate for the middle of the anchor.
     * @param popupStartCoord x-coordinate for the popup start
     * @param arrowIndicatorWidth Width of the arrow indicator.
     */
    @Composable
    private fun computeIndicatorArrowStartCoord(
        anchorMiddleXCoord: Pixels,
        popupStartCoord: Pixels,
        arrowIndicatorWidth: Pixels
    ): Pixels {
        val arrowIndicatorHalfWidth = arrowIndicatorWidth.value / 2

        return if (LocalConfiguration.current.layoutDirection == View.LAYOUT_DIRECTION_LTR) {
            val visiblePopupStartCoord = popupStartCoord.value + HORIZONTAL_PADDING.dp.toPx()
            val arrowIndicatorStartCoord = anchorMiddleXCoord.value - arrowIndicatorHalfWidth

            Pixels((visiblePopupStartCoord - arrowIndicatorStartCoord).absoluteValue)
        } else {
            val indicatorStartCoord = popupStartCoord.value - HORIZONTAL_PADDING.dp.toPx() -
                anchorMiddleXCoord.value - arrowIndicatorHalfWidth

            Pixels(indicatorStartCoord.absoluteValue)
        }
    }

    /**
     * Cleanup and remove the current popup from the screen.
     * Clients are not automatically informed about this. Use a separate call to [onDismiss] if needed.
     */
    @VisibleForTesting
    internal fun dismiss() {
        anchor.removeOnAttachStateChangeListener(anchorDetachedListener)
        GeckoScreenOrientation.getInstance().removeListener(orientationChangeListener)
        disposeComposition()
        ViewTreeLifecycleOwner.set(this, null)
        ViewTreeSavedStateRegistryOwner.set(this, null)
        windowManager.removeViewImmediate(this)
    }

    /**
     * Create fullscreen translucent layout params.
     * This will allow placing the visible popup anywhere on the screen.
     */
    @VisibleForTesting internal fun createLayoutParams(): WindowManager.LayoutParams =
        WindowManager.LayoutParams().apply {
            type = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL
            token = container.applicationWindowToken
            width = WindowManager.LayoutParams.MATCH_PARENT
            height = WindowManager.LayoutParams.MATCH_PARENT
            format = PixelFormat.TRANSLUCENT
            flags = WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
                WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED
        }

    /**
     * Intended to allow querying the insets of the navigation bar.
     * Value will be `0` except for when the screen is rotated by 90 degrees.
     */
    private fun getLeftInsets() = ViewCompat.getRootWindowInsets(container)
        ?.getInsets(WindowInsetsCompat.Type.systemBars())?.left
        ?: 0.coerceAtLeast(0)

    @Px
    private fun Dp.toPx(): Int {
        return this.value
            .dpToPx(container.resources.displayMetrics)
            .roundToInt()
    }
}

/**
 * Simpler [View.OnAttachStateChangeListener] only informing about
 * [View.OnAttachStateChangeListener.onViewDetachedFromWindow].
 */
private class OnViewDetachedListener(val onDismiss: () -> Unit) : View.OnAttachStateChangeListener {
    override fun onViewAttachedToWindow(v: View?) {
        // no-op
    }

    override fun onViewDetachedFromWindow(v: View?) {
        onDismiss()
    }
}

/**
 * Complete content of the popup.
 * [CFRPopupShape] with a gradient background containing [text] and a dismiss ("X") button.
 *
 * @param text String offering more context about a particular feature to the user.
 * @param indicatorArrowStartOffset Maximum distance between the popup start and the indicator arrow.
 * If there isn't enough space this could automatically be overridden up to 0.
 * @param indicatorArrowHeight How tall the indicator arrow should be.
 * This will also affect how wide the base of the indicator arrow will be.
 * @param elevation The z-coordinate at which to place the popup.
 * This controls the size of the shadow below the popup.
 * @param onDismissButton Callback for when the user clicks on the "X" button.
 */
@Composable
private fun CFRPopupContent(
    text: String,
    indicatorArrowStartOffset: Dp,
    indicatorArrowHeight: Dp,
    elevation: Dp = 0.dp,
    onDismissButton: () -> Unit
) {
    FocusTheme {
        Box(modifier = Modifier.width(POPUP_WIDTH_DP.dp + HORIZONTAL_PADDING.dp * 2)) {
            Surface(
                color = Color.Transparent,
                elevation = elevation,
                // Need to override the default RectangleShape to avoid casting shadows for that shape.
                shape = CFRPopupShape(indicatorArrowStartOffset, 10.dp, 10.dp),
                modifier = Modifier
                    .align(Alignment.Center)
                    .background(
                        shape = CFRPopupShape(indicatorArrowStartOffset, 10.dp, 10.dp),
                        brush = Brush.linearGradient(
                            colors = listOf(
                                colorResource(color.cfr_pop_up_shape_end_color),
                                colorResource(color.cfr_pop_up_shape_start_color)
                            ),
                            end = Offset(0f, Float.POSITIVE_INFINITY),
                            start = Offset(Float.POSITIVE_INFINITY, 0f)
                        )
                    )
                    .wrapContentHeight()
                    .width(POPUP_WIDTH_DP.dp)
            ) {
                Text(
                    text = text,
                    fontSize = 16.sp,
                    letterSpacing = 0.5.sp,
                    color = colorResource(color.cfr_text_color),
                    lineHeight = 24.sp,
                    modifier = Modifier
                        .padding(
                            start = 16.dp,
                            top = 16.dp + indicatorArrowHeight,
                            end = 33.dp,
                            bottom = 16.dp
                        )
                )
            }

            IconButton(
                onClick = onDismissButton,
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(top = 6.dp, end = 6.dp)
                    .size(48.dp)
            ) {
                Icon(
                    Icons.Filled.Close,
                    contentDescription = stringResource(id = R.string.cfr_close_button_description),
                    modifier = Modifier.size(22.dp),
                    tint = colorResource(color.cardview_light_background)
                )
            }
        }
    }
}

@Preview
@Composable
private fun CFRPopupPreview() {
    FocusTheme {
        CFRPopupContent(
            text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt",
            indicatorArrowStartOffset = 10.dp,
            indicatorArrowHeight = 15.dp,
            elevation = 30.dp,
            onDismissButton = { }
        )
    }
}
