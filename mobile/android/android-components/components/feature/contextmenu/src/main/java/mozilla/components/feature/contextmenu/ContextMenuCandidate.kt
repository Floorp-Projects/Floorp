/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.view.View
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.base.observer.Consumable

/**
 * A candidate for an item to be displayed in the context menu.
 *
 * @property id A unique ID that will be used to uniquely identify the candidate that the user selected.
 * @property label The label that will be displayed in the context menu
 * @property showFor If this lambda returns true for a given [Session] and [HitResult] then it will be displayed in the
 * context menu.
 * @property action The action to be invoked once the user selects this item.
 */
data class ContextMenuCandidate(
    val id: String,
    val label: String,
    val showFor: (Session, HitResult) -> Boolean,
    val action: (Session, HitResult) -> Unit
) {
    companion object {
        /**
         * Returns the default list of context menu candidates.
         *
         * Use this list if you do not intend to customize the context menu items to be displayed.
         */
        fun defaultCandidates(
            context: Context,
            tabsUseCases: TabsUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ): List<ContextMenuCandidate> = listOf(
            createOpenInNewTabCandidate(context, tabsUseCases, snackBarParentView, snackbarDelegate),
            createOpenInPrivateTabCandidate(context, tabsUseCases, snackBarParentView, snackbarDelegate),
            createCopyLinkCandidate(context, snackBarParentView, snackbarDelegate),
            createShareLinkCandidate(context),
            createOpenImageInNewTabCandidate(context, tabsUseCases, snackBarParentView, snackbarDelegate),
            createSaveImageCandidate(context),
            createCopyImageLocationCandidate(context, snackBarParentView, snackbarDelegate)
        )

        /**
         * Context Menu item: "Open Link in New Tab".
         */
        fun createOpenInNewTabCandidate(
            context: Context,
            tabsUseCases: TabsUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_in_new_tab",
            label = context.getString(R.string.mozac_feature_contextmenu_open_link_in_new_tab),
            showFor = { session, hitResult -> hitResult.isLink() && !session.private },
            action = { parent, hitResult ->
                val session = tabsUseCases.addTab(
                    hitResult.getLink(), selectTab = false, startLoading = true, parent = parent)

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_new_tab_opened,
                    duration = Snackbar.LENGTH_LONG,
                    action = R.string.mozac_feature_contextmenu_snackbar_action_switch
                ) {
                    tabsUseCases.selectTab(session)
                }
            }
        )

        /**
         * Context Menu item: "Open Link in Private Tab".
         */
        fun createOpenInPrivateTabCandidate(
            context: Context,
            tabsUseCases: TabsUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_in_private_tab",
            label = context.getString(R.string.mozac_feature_contextmenu_open_link_in_private_tab),
            showFor = { _, hitResult -> hitResult.isLink() },
            action = { parent, hitResult ->
                val session = tabsUseCases.addPrivateTab(
                    hitResult.getLink(), selectTab = false, startLoading = true, parent = parent)

                snackbarDelegate.show(
                    snackBarParentView,
                    R.string.mozac_feature_contextmenu_snackbar_new_private_tab_opened,
                    Snackbar.LENGTH_LONG,
                    R.string.mozac_feature_contextmenu_snackbar_action_switch
                ) {
                    tabsUseCases.selectTab(session)
                }
            }
        )

        /**
         * Context Menu item: "Open Image in New Tab".
         */
        fun createOpenImageInNewTabCandidate(
            context: Context,
            tabsUseCases: TabsUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_image_in_new_tab",
            label = context.getString(R.string.mozac_feature_contextmenu_open_image_in_new_tab),
            showFor = { _, hitResult -> hitResult.isImage() },
            action = { parent, hitResult ->
                val session = if (parent.private) {
                    tabsUseCases.addPrivateTab(
                        hitResult.src, selectTab = false, startLoading = true, parent = parent)
                } else {
                    tabsUseCases.addTab(
                        hitResult.src, selectTab = false, startLoading = true, parent = parent)
                }

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_new_tab_opened,
                    duration = Snackbar.LENGTH_LONG,
                    action = R.string.mozac_feature_contextmenu_snackbar_action_switch
                ) {
                    tabsUseCases.selectTab(session)
                }
            }
        )

        /**
         * Context Menu item: "Save image".
         */
        fun createSaveImageCandidate(
            context: Context
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.save_image",
            label = context.getString(R.string.mozac_feature_contextmenu_save_image),
            showFor = { _, hitResult -> hitResult.isImage() },
            action = { session, hitResult ->
                session.download = Consumable.from(Download(hitResult.src))
            }
        )

        /**
         * Context Menu item: "Share Link".
         */
        fun createShareLinkCandidate(
            context: Context
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.share_link",
            label = context.getString(R.string.mozac_feature_contextmenu_share_link),
            showFor = { _, hitResult -> hitResult.isLink() },
            action = { _, hitResult ->
                val intent = Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    putExtra(Intent.EXTRA_TEXT, hitResult.getLink())
                }

                val shareIntent = Intent.createChooser(
                    intent,
                    context.getString(R.string.mozac_feature_contextmenu_share_link)
                ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)

                context.startActivity(shareIntent)
            }
        )

        /**
         * Context Menu item: "Copy Link".
         */
        fun createCopyLinkCandidate(
            context: Context,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.copy_link",
            label = context.getString(R.string.mozac_feature_contextmenu_copy_link),
            showFor = { _, hitResult -> hitResult.isLink() },
            action = { _, hitResult ->
                val clipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clip = ClipData.newPlainText(hitResult.getLink(), hitResult.getLink())
                clipboardManager.setPrimaryClip(clip)

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_text_copied,
                    duration = Snackbar.LENGTH_SHORT
                )
            }
        )

        /**
         * Context Menu item: "Copy Image Location".
         */
        fun createCopyImageLocationCandidate(
            context: Context,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.copy_image_location",
            label = context.getString(R.string.mozac_feature_contextmenu_copy_image_location),
            showFor = { _, hitResult -> hitResult.isImage() },
            action = { _, hitResult ->
                val clipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                val clip = ClipData.newPlainText(hitResult.getLink(), hitResult.src)
                clipboardManager.setPrimaryClip(clip)

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_text_copied,
                    duration = Snackbar.LENGTH_SHORT
                )
            }
        )
    }

    /**
     * Delegate to display a snackbar.
     */
    interface SnackbarDelegate {
        /**
         * Displays a snackbar.
         *
         * @param snackBarParentView The view to find a parent from for displaying the Snackbar.
         * @param text The text to show. Can be formatted text.
         * @param duration How long to display the message
         * @param action String resource to display for the action.
         * @param listener callback to be invoked when the action is clicked
         */
        fun show(
            snackBarParentView: View,
            text: Int,
            duration: Int,
            action: Int = 0,
            listener: ((v: View) -> Unit)? = null
        )
    }
}

// Some helper methods to work with HitResult. We may want to improve the API of HitResult and remove some of the
// helpers eventually: https://github.com/mozilla-mobile/android-components/issues/1443

private fun HitResult.isImage(): Boolean =
    (this is HitResult.IMAGE || this is HitResult.IMAGE_SRC) && src.isNotEmpty()

private fun HitResult.isLink(): Boolean =
    ((this is HitResult.UNKNOWN && src.isNotEmpty()) || this is HitResult.IMAGE_SRC) &&
        getLink().startsWith("http")

internal fun HitResult.getLink(): String = when (this) {
    is HitResult.UNKNOWN -> src
    is HitResult.IMAGE_SRC -> uri
    is HitResult.IMAGE ->
        if (title.isNullOrBlank()) src else title.toString()
    else -> "about:blank"
}

/**
 * Default implementation for [ContextMenuCandidate.SnackbarDelegate]. Will display a standard default Snackbar.
 */
class DefaultSnackbarDelegate : ContextMenuCandidate.SnackbarDelegate {
    override fun show(
        snackBarParentView: View,
        text: Int,
        duration: Int,
        action: Int,
        listener: ((v: View) -> Unit)?
    ) {
        val snackbar = Snackbar.make(
            snackBarParentView,
            text,
            duration)

        if (action != 0 && listener != null) {
            snackbar.setAction(action, listener)
        }

        snackbar.show()
    }
}
