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
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.support.ktx.android.content.addContact
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.kotlin.stripMailToProtocol

/**
 * A candidate for an item to be displayed in the context menu.
 *
 * @property id A unique ID that will be used to uniquely identify the candidate that the user selected.
 * @property label The label that will be displayed in the context menu
 * @property showFor If this lambda returns true for a given [SessionState] and [HitResult] then it
 * will be displayed in the context menu.
 * @property action The action to be invoked once the user selects this item.
 */
@Suppress("TooManyFunctions")
data class ContextMenuCandidate(
    val id: String,
    val label: String,
    val showFor: (SessionState, HitResult) -> Boolean,
    val action: (SessionState, HitResult) -> Unit
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
            contextMenuUseCases: ContextMenuUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ): List<ContextMenuCandidate> = listOf(
            createOpenInNewTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate
            ),
            createOpenInPrivateTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate
            ),
            createCopyLinkCandidate(context, snackBarParentView, snackbarDelegate),
            createDownloadLinkCandidate(context, contextMenuUseCases),
            createShareLinkCandidate(context),
            createShareImageCandidate(context),
            createOpenImageInNewTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate
            ),
            createSaveImageCandidate(context, contextMenuUseCases),
            createSaveVideoAudioCandidate(context, contextMenuUseCases),
            createCopyImageLocationCandidate(context, snackBarParentView, snackbarDelegate),
            createAddContactCandidate(context),
            createShareEmailAddressCandidate(context),
            createCopyEmailAddressCandidate(context, snackBarParentView, snackbarDelegate)
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
            showFor = { tab, hitResult -> hitResult.isLink() && !tab.content.private },
            action = { parent, hitResult ->
                val tab = tabsUseCases.addTab(
                    hitResult.getLink(),
                    selectTab = false,
                    startLoading = true,
                    parentId = parent.id,
                    contextId = parent.contextId
                )

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_new_tab_opened,
                    duration = Snackbar.LENGTH_LONG,
                    action = R.string.mozac_feature_contextmenu_snackbar_action_switch
                ) {
                    tabsUseCases.selectTab(tab)
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
                val tab = tabsUseCases.addPrivateTab(
                    hitResult.getLink(),
                    selectTab = false,
                    startLoading = true,
                    parentId = parent.id
                )

                snackbarDelegate.show(
                    snackBarParentView,
                    R.string.mozac_feature_contextmenu_snackbar_new_private_tab_opened,
                    Snackbar.LENGTH_LONG,
                    R.string.mozac_feature_contextmenu_snackbar_action_switch
                ) {
                    tabsUseCases.selectTab(tab)
                }
            }
        )

        /**
         * Context Menu item: "Open Link in external App".
         */
        fun createOpenInExternalAppCandidate(
            context: Context,
            appLinksUseCases: AppLinksUseCases
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_in_external_app",
            label = context.getString(R.string.mozac_feature_contextmenu_open_link_in_external_app),
            showFor = { _, hitResult -> hitResult.canOpenInExternalApp(appLinksUseCases) },
            action = { _, hitResult ->
                val link = hitResult.getLink()
                val redirect = appLinksUseCases.appLinkRedirectIncludeInstall(link)
                val appIntent = redirect.appIntent
                val marketPlaceIntent = redirect.marketplaceIntent
                if (appIntent != null) {
                    appLinksUseCases.openAppLink(appIntent)
                } else if (marketPlaceIntent != null) {
                    appLinksUseCases.openAppLink(marketPlaceIntent)
                }
            }
        )

        /**
         * Context Menu item: "Add to contact".
         */
        fun createAddContactCandidate(
            context: Context
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.add_to_contact",
            label = context.getString(R.string.mozac_feature_contextmenu_add_to_contact),
            showFor = { _, hitResult -> hitResult.isMailto() },
            action = { _, hitResult -> context.addContact(hitResult.getLink().stripMailToProtocol()) }
        )

        /**
         * Context Menu item: "Share email address".
         */
        fun createShareEmailAddressCandidate(
            context: Context
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.share_email",
            label = context.getString(R.string.mozac_feature_contextmenu_share_email_address),
            showFor = { _, hitResult -> hitResult.isMailto() },
            action = { _, hitResult -> context.share(hitResult.getLink().stripMailToProtocol()) }
        )

        /**
         * Context Menu item: "Copy email address".
         */
        fun createCopyEmailAddressCandidate(
            context: Context,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.copy_email_address",
            label = context.getString(R.string.mozac_feature_contextmenu_copy_email_address),
            showFor = { _, hitResult -> hitResult.isMailto() },
            action = { _, hitResult ->
                val email = hitResult.getLink().stripMailToProtocol()
                clipPlaintText(context, email, email,
                    R.string.mozac_feature_contextmenu_snackbar_email_address_copied, snackBarParentView,
                    snackbarDelegate)
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
                val tab = if (parent.content.private) {
                    tabsUseCases.addPrivateTab(
                        hitResult.src, selectTab = false, startLoading = true, parentId = parent.id
                    )
                } else {
                    tabsUseCases.addTab(
                        hitResult.src,
                        selectTab = false,
                        startLoading = true,
                        parentId = parent.id,
                        contextId = parent.contextId
                    )
                }

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_new_tab_opened,
                    duration = Snackbar.LENGTH_LONG,
                    action = R.string.mozac_feature_contextmenu_snackbar_action_switch
                ) {
                    tabsUseCases.selectTab(tab)
                }
            }
        )

        /**
         * Context Menu item: "Save image".
         */
        fun createSaveImageCandidate(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.save_image",
            label = context.getString(R.string.mozac_feature_contextmenu_save_image),
            showFor = { _, hitResult -> hitResult.isImage() },
            action = { tab, hitResult ->
                contextMenuUseCases.injectDownload(
                    tab.id,
                    DownloadState(hitResult.src, skipConfirmation = true)
                )
            }
        )

        /**
         * Context Menu item: "Save video".
         */
        fun createSaveVideoAudioCandidate(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.save_video",
            label = context.getString(R.string.mozac_feature_contextmenu_save_file_to_device),
            showFor = { _, hitResult -> hitResult.isVideoAudio() },
            action = { tab, hitResult ->
                contextMenuUseCases.injectDownload(
                    tab.id,
                    DownloadState(hitResult.src, skipConfirmation = true)
                )
            }
        )

        /**
         * Context Menu item: "Save link".
         */
        fun createDownloadLinkCandidate(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.download_link",
            label = context.getString(R.string.mozac_feature_contextmenu_download_link),
            showFor = { _, hitResult -> hitResult.isLink() },
            action = { tab, hitResult ->
                contextMenuUseCases.injectDownload(
                    tab.id,
                    DownloadState(hitResult.src, skipConfirmation = true)
                )
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
         * Context Menu item: "Share image"
         */
        fun createShareImageCandidate(
            context: Context,
            action: (SessionState, HitResult) -> Unit = { _, hitResult -> context.share(hitResult.src) }
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.share_image",
            label = context.getString(R.string.mozac_feature_contextmenu_share_image),
            showFor = { _, hitResult -> hitResult.isImage() },
            action = action
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
                clipPlaintText(context, hitResult.getLink(), hitResult.getLink(),
                    R.string.mozac_feature_contextmenu_snackbar_link_copied, snackBarParentView,
                    snackbarDelegate)
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
                clipPlaintText(context, hitResult.getLink(), hitResult.src,
                    R.string.mozac_feature_contextmenu_snackbar_link_copied, snackBarParentView,
                    snackbarDelegate)
            }
        )

        @Suppress("LongParameterList")
        private fun clipPlaintText(
            context: Context,
            label: String,
            plainText: String,
            displayTextId: Int,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate()
        ) {
            val clipboardManager =
                context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText(label, plainText)
            clipboardManager.setPrimaryClip(clip)

            snackbarDelegate.show(
                snackBarParentView = snackBarParentView,
                text = displayTextId,
                duration = Snackbar.LENGTH_SHORT
            )
        }
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

private fun HitResult.isVideoAudio(): Boolean =
    (this is HitResult.VIDEO || this is HitResult.AUDIO) && src.isNotEmpty()

private fun HitResult.isLink(): Boolean =
    ((this is HitResult.UNKNOWN && src.isNotEmpty()) || this is HitResult.IMAGE_SRC) &&
        getLink().startsWith("http")

private fun HitResult.isIntent(): Boolean =
    (this is HitResult.UNKNOWN && src.isNotEmpty() &&
        getLink().startsWith("intent:"))

private fun HitResult.isMailto(): Boolean =
    (this is HitResult.UNKNOWN && src.isNotEmpty()) &&
        getLink().startsWith("mailto:")

private fun HitResult.canOpenInExternalApp(appLinksUseCases: AppLinksUseCases): Boolean {
    if (isLink() || isIntent() || isVideoAudio()) {
        val redirect = appLinksUseCases.appLinkRedirectIncludeInstall(getLink())
        return redirect.hasExternalApp() || redirect.hasMarketplaceIntent()
    }
    return false
}

internal fun HitResult.getLink(): String = when (this) {
    is HitResult.UNKNOWN -> src
    is HitResult.IMAGE_SRC -> uri
    is HitResult.IMAGE ->
        if (title.isNullOrBlank()) src else title.toString()
    is HitResult.VIDEO ->
        if (title.isNullOrBlank()) src else title.toString()
    is HitResult.AUDIO ->
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
            duration
        )

        if (action != 0 && listener != null) {
            snackbar.setAction(action, listener)
        }

        snackbar.show()
    }
}
