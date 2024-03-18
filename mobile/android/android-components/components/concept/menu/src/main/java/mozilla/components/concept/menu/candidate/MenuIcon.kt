/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.candidate

import android.content.Context
import android.graphics.drawable.Drawable
import androidx.annotation.ColorInt
import androidx.annotation.DrawableRes
import androidx.appcompat.content.res.AppCompatResources

/**
 * Menu option data classes to be shown alongside menu options
 */
sealed class MenuIcon

/**
 * Menu icon that displays a drawable.
 *
 * @property drawable Drawable icon to display.
 * @property tint Tint to apply to the drawable.
 * @property effect Effects to apply to the icon.
 */
data class DrawableMenuIcon(
    override val drawable: Drawable?,
    @ColorInt override val tint: Int? = null,
    val effect: MenuIconEffect? = null,
) : MenuIcon(), MenuIconWithDrawable {

    constructor(
        context: Context,
        @DrawableRes resource: Int,
        @ColorInt tint: Int? = null,
        effect: MenuIconEffect? = null,
    ) : this(AppCompatResources.getDrawable(context, resource), tint, effect)
}

/**
 * Menu icon that displays an image button.
 *
 * @property drawable Drawable icon to display.
 * @property tint Tint to apply to the drawable.
 * @property onClick Click listener called when this menu option is clicked.
 */
data class DrawableButtonMenuIcon(
    override val drawable: Drawable?,
    @ColorInt override val tint: Int? = null,
    val onClick: () -> Unit = {},
) : MenuIcon(), MenuIconWithDrawable {

    constructor(
        context: Context,
        @DrawableRes resource: Int,
        @ColorInt tint: Int? = null,
        onClick: () -> Unit = {},
    ) : this(AppCompatResources.getDrawable(context, resource), tint, onClick)
}

/**
 * Menu icon that displays a drawable.
 *
 * @property loadDrawable Function that creates drawable icon to display.
 * @property loadingDrawable Drawable that is displayed while loadDrawable is running.
 * @property fallbackDrawable Drawable that is displayed if loadDrawable fails.
 * @property tint Tint to apply to the drawable.
 * @property effect Effects to apply to the icon.
 */
data class AsyncDrawableMenuIcon(
    val loadDrawable: suspend (width: Int, height: Int) -> Drawable?,
    val loadingDrawable: Drawable? = null,
    val fallbackDrawable: Drawable? = null,
    @ColorInt val tint: Int? = null,
    val effect: MenuIconEffect? = null,
) : MenuIcon()

/**
 * Menu icon to display additional text at the end of a menu option.
 *
 * @property text Text to display.
 * @property backgroundTint Color to show behind text.
 * @property textStyle Styling to apply to the text.
 */
data class TextMenuIcon(
    val text: String,
    @ColorInt val backgroundTint: Int? = null,
    val textStyle: TextStyle = TextStyle(),
) : MenuIcon()

/**
 * Interface shared by all [MenuIcon]s with drawables.
 */
interface MenuIconWithDrawable {
    val drawable: Drawable?

    @get:ColorInt val tint: Int?
}
