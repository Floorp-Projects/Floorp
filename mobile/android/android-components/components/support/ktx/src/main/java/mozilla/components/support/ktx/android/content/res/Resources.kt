package mozilla.components.support.ktx.android.content.res

import android.content.res.Resources
import android.util.TypedValue

/**
 * Converts a value in density independent pixels (pxToDp) to the actual pixel values for the display.
 * @deprecated use Int.dpToPx instead.
 */
// @Deprecated("Use Int.dpToPx instead",
// ReplaceWith("pixels.dpToPx(this.displayMetrics)", "mozilla.components.support.ktx.android.util.Int.dpToPx"))
fun Resources.pxToDp(pixels: Int) = TypedValue.applyDimension(
        TypedValue.COMPLEX_UNIT_DIP, pixels.toFloat(), displayMetrics).toInt()
