/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.content.ClipData
import android.content.ClipboardManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.LabeledIntent
import android.net.Uri
import android.os.Build
import android.os.Parcelable
import android.view.View
import androidx.annotation.VisibleForTesting
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.concept.engine.HitResult
import mozilla.components.feature.app.links.AppLinksUseCases
import mozilla.components.feature.contextmenu.ContextMenuCandidate.Companion.MAX_TITLE_LENGTH
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.ktx.android.content.addContact
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.kotlin.stripMailToProtocol
import mozilla.components.support.ktx.kotlin.takeOrReplace

/**
 * A candidate for an item to be displayed in the context menu.
 *
 * @property id A unique ID that will be used to uniquely identify the candidate that the user selected.
 * @property label The label that will be displayed in the context menu
 * @property showFor If this lambda returns true for a given [SessionState] and [HitResult] then it
 * will be displayed in the context menu.
 * @property action The action to be invoked once the user selects this item.
 */
data class ContextMenuCandidate(
    val id: String,
    val label: String,
    val showFor: (SessionState, HitResult) -> Boolean,
    val action: (SessionState, HitResult) -> Unit,
) {
    companion object {
        // This is used for limiting image title, in order to prevent crashes caused by base64 encoded image
        // https://github.com/mozilla-mobile/android-components/issues/8298
        const val MAX_TITLE_LENGTH = 2500

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
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
        ): List<ContextMenuCandidate> = listOf(
            createOpenInNewTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate,
            ),
            createOpenInPrivateTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate,
            ),
            createCopyLinkCandidate(context, snackBarParentView, snackbarDelegate),
            createDownloadLinkCandidate(context, contextMenuUseCases),
            createShareLinkCandidate(context),
            createShareImageCandidate(context, contextMenuUseCases),
            createOpenImageInNewTabCandidate(
                context,
                tabsUseCases,
                snackBarParentView,
                snackbarDelegate,
            ),
            createSaveImageCandidate(context, contextMenuUseCases),
            createSaveVideoAudioCandidate(context, contextMenuUseCases),
            createCopyImageLocationCandidate(context, snackBarParentView, snackbarDelegate),
            createAddContactCandidate(context),
            createShareEmailAddressCandidate(context),
            createCopyEmailAddressCandidate(context, snackBarParentView, snackbarDelegate),
        )

        /**
         * Context Menu item: "Open Link in New Tab".
         *
         * @param context [Context] used for various system interactions.
         * @param tabsUseCases [TabsUseCases] used for adding new tabs.
         * @param snackBarParentView The view in which to find a suitable parent for displaying the `Snackbar`.
         * @param snackbarDelegate [SnackbarDelegate] used to actually show a `Snackbar`.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createOpenInNewTabCandidate(
            context: Context,
            tabsUseCases: TabsUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_in_new_tab",
            label = context.getString(R.string.mozac_feature_contextmenu_open_link_in_new_tab),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isHttpLink() &&
                    !tab.content.private &&
                    additionalValidation(tab, hitResult)
            },
            action = { parent, hitResult ->
                val tab = tabsUseCases.addTab(
                    hitResult.getLink(),
                    selectTab = false,
                    startLoading = true,
                    parentId = parent.id,
                    contextId = parent.contextId,
                )

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_new_tab_opened,
                    duration = Snackbar.LENGTH_LONG,
                    action = R.string.mozac_feature_contextmenu_snackbar_action_switch,
                ) {
                    tabsUseCases.selectTab(tab)
                }
            },
        )

        /**
         * Context Menu item: "Open Link in Private Tab".
         *
         * @param context [Context] used for various system interactions.
         * @param tabsUseCases [TabsUseCases] used for adding new tabs.
         * @param snackBarParentView The view in which to find a suitable parent for displaying the `Snackbar`.
         * @param snackbarDelegate [SnackbarDelegate] used to actually show a `Snackbar`.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.         */
        fun createOpenInPrivateTabCandidate(
            context: Context,
            tabsUseCases: TabsUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_in_private_tab",
            label = context.getString(R.string.mozac_feature_contextmenu_open_link_in_private_tab),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isHttpLink() &&
                    additionalValidation(tab, hitResult)
            },
            action = { parent, hitResult ->
                val tab = tabsUseCases.addTab(
                    hitResult.getLink(),
                    selectTab = false,
                    startLoading = true,
                    parentId = parent.id,
                    private = true,
                )

                snackbarDelegate.show(
                    snackBarParentView,
                    R.string.mozac_feature_contextmenu_snackbar_new_private_tab_opened,
                    Snackbar.LENGTH_LONG,
                    R.string.mozac_feature_contextmenu_snackbar_action_switch,
                ) {
                    tabsUseCases.selectTab(tab)
                }
            },
        )

        /**
         * Context Menu item: "Open Link in external App".
         *
         * @param context [Context] used for various system interactions.
         * @param appLinksUseCases [AppLinksUseCases] used to interact with urls that can be opened in 3rd party apps.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createOpenInExternalAppCandidate(
            context: Context,
            appLinksUseCases: AppLinksUseCases,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_in_external_app",
            label = context.getString(R.string.mozac_feature_contextmenu_open_link_in_external_app),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.canOpenInExternalApp(appLinksUseCases) &&
                    additionalValidation(tab, hitResult)
            },
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
            },
        )

        /**
         * Context Menu item: "Add to contact".
         *
         * @param context [Context] used for various system interactions.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createAddContactCandidate(
            context: Context,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.add_to_contact",
            label = context.getString(R.string.mozac_feature_contextmenu_add_to_contact),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isMailto() &&
                    additionalValidation(tab, hitResult)
            },
            action = { _, hitResult -> context.addContact(hitResult.getLink().stripMailToProtocol()) },
        )

        /**
         * Context Menu item: "Share email address".
         *
         * @param context [Context] used for various system interactions.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createShareEmailAddressCandidate(
            context: Context,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.share_email",
            label = context.getString(R.string.mozac_feature_contextmenu_share_email_address),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isMailto() &&
                    additionalValidation(tab, hitResult)
            },
            action = { _, hitResult -> context.share(hitResult.getLink().stripMailToProtocol()) },
        )

        /**
         * Context Menu item: "Copy email address".
         *
         * @param context [Context] used for various system interactions.
         * @param snackBarParentView The view in which to find a suitable parent for displaying the `Snackbar`.
         * @param snackbarDelegate [SnackbarDelegate] used to actually show a `Snackbar`.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createCopyEmailAddressCandidate(
            context: Context,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.copy_email_address",
            label = context.getString(R.string.mozac_feature_contextmenu_copy_email_address),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isMailto() &&
                    additionalValidation(tab, hitResult)
            },
            action = { _, hitResult ->
                val email = hitResult.getLink().stripMailToProtocol()
                clipPlaintText(
                    context,
                    email,
                    email,
                    R.string.mozac_feature_contextmenu_snackbar_email_address_copied,
                    snackBarParentView,
                    snackbarDelegate,
                )
            },
        )

        /**
         * Context Menu item: "Open Image in New Tab".
         *
         * @param context [Context] used for various system interactions.
         * @param tabsUseCases [TabsUseCases] used for adding new tabs.
         * @param snackBarParentView The view in which to find a suitable parent for displaying the `Snackbar`.
         * @param snackbarDelegate [SnackbarDelegate] used to actually show a `Snackbar`.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createOpenImageInNewTabCandidate(
            context: Context,
            tabsUseCases: TabsUseCases,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.open_image_in_new_tab",
            label = context.getString(R.string.mozac_feature_contextmenu_open_image_in_new_tab),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isImage() &&
                    additionalValidation(tab, hitResult)
            },
            action = { parent, hitResult ->
                val tab = tabsUseCases.addTab(
                    hitResult.src,
                    selectTab = false,
                    startLoading = true,
                    parentId = parent.id,
                    contextId = parent.contextId,
                    private = parent.content.private,
                )

                snackbarDelegate.show(
                    snackBarParentView = snackBarParentView,
                    text = R.string.mozac_feature_contextmenu_snackbar_new_tab_opened,
                    duration = Snackbar.LENGTH_LONG,
                    action = R.string.mozac_feature_contextmenu_snackbar_action_switch,
                ) {
                    tabsUseCases.selectTab(tab)
                }
            },
        )

        /**
         * Context Menu item: "Save image".
         *
         * @param context [Context] used for various system interactions.
         * @param contextMenuUseCases [ContextMenuUseCases] used to integrate other features.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createSaveImageCandidate(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.save_image",
            label = context.getString(R.string.mozac_feature_contextmenu_save_image),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isImage() &&
                    additionalValidation(tab, hitResult)
            },
            action = { tab, hitResult ->
                contextMenuUseCases.injectDownload(
                    tab.id,
                    DownloadState(hitResult.src, skipConfirmation = true, private = tab.content.private),
                )
            },
        )

        /**
         * Context Menu item: "Save video".
         *
         * @param context [Context] used for various system interactions.
         * @param contextMenuUseCases [ContextMenuUseCases] used to integrate other features.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createSaveVideoAudioCandidate(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.save_video",
            label = context.getString(R.string.mozac_feature_contextmenu_save_file_to_device),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isVideoAudio() &&
                    additionalValidation(tab, hitResult)
            },
            action = { tab, hitResult ->
                contextMenuUseCases.injectDownload(
                    tab.id,
                    DownloadState(hitResult.src, skipConfirmation = true, private = tab.content.private),
                )
            },
        )

        /**
         * Context Menu item: "Save link".
         *
         * @param context [Context] used for various system interactions.
         * @param contextMenuUseCases [ContextMenuUseCases] used to integrate other features.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createDownloadLinkCandidate(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.download_link",
            label = context.getString(R.string.mozac_feature_contextmenu_download_link),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isLinkForOtherThanWebpage() &&
                    additionalValidation(tab, hitResult)
            },
            action = { tab, hitResult ->
                contextMenuUseCases.injectDownload(
                    tab.id,
                    DownloadState(hitResult.src, skipConfirmation = true, private = tab.content.private),
                )
            },
        )

        /**
         * Context Menu item: "Share Link".
         *
         * @param context [Context] used for various system interactions.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createShareLinkCandidate(
            context: Context,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.share_link",
            label = context.getString(R.string.mozac_feature_contextmenu_share_link),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    (hitResult.isUri() || hitResult.isImage() || hitResult.isVideoAudio()) &&
                    additionalValidation(tab, hitResult)
            },
            action = { _, hitResult ->
                val intent = Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    putExtra(Intent.EXTRA_TEXT, hitResult.getLink())
                }
                context.startActivity(
                    createShareIntent(
                        intent,
                        context,
                        context.getString(R.string.mozac_feature_contextmenu_share_link),
                    ),
                )
            },
        )

        /**
         * Create a "Share link" intent chooser excluding the current app
         */
        private fun createShareIntent(
            intent: Intent,
            context: Context,
            title: CharSequence,
        ): Intent {
            val chooserIntent: Intent
            val resolveInfos = context.packageManager.queryIntentActivities(intent, 0).toHashSet()

            val excludedComponentNames = resolveInfos
                .map { it.activityInfo }
                .filter { it.packageName == context.packageName }
                .map { ComponentName(it.packageName, it.name) }

            // Starting with Android N we can use Intent.EXTRA_EXCLUDE_COMPONENTS to exclude components
            // other way we are constrained to use Intent.EXTRA_INITIAL_INTENTS.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                chooserIntent = Intent.createChooser(intent, title)
                    .putExtra(
                        Intent.EXTRA_EXCLUDE_COMPONENTS,
                        excludedComponentNames.toTypedArray(),
                    )
            } else {
                var targetIntents = resolveInfos
                    .filterNot { it.activityInfo.packageName == context.packageName }
                    .map { resolveInfo ->
                        val activityInfo = resolveInfo.activityInfo
                        val targetIntent = Intent(intent).apply {
                            component = ComponentName(activityInfo.packageName, activityInfo.name)
                        }
                        LabeledIntent(
                            targetIntent,
                            activityInfo.packageName,
                            resolveInfo.labelRes,
                            resolveInfo.icon,
                        )
                    }

                // Sometimes on Android M and below an empty chooser is displayed, problem reported also here
                // https://issuetracker.google.com/issues/37085761
                // To fix that we are creating a chooser with an empty intent
                chooserIntent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    Intent.createChooser(Intent(), title)
                } else {
                    targetIntents = targetIntents.toMutableList()
                    Intent.createChooser(targetIntents.removeAt(0), title)
                }
                chooserIntent.putExtra(
                    Intent.EXTRA_INITIAL_INTENTS,
                    targetIntents.toTypedArray<Parcelable>(),
                )
            }
            chooserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            return chooserIntent
        }

        /**
         * Context Menu item: "Share image"
         *
         * @param context [Context] used for various system interactions.
         * @param contextMenuUseCases [ContextMenuUseCases] used to integrate other features.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createShareImageCandidate(
            context: Context,
            contextMenuUseCases: ContextMenuUseCases,
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.share_image",
            label = context.getString(R.string.mozac_feature_contextmenu_share_image),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isImage() &&
                    additionalValidation(tab, hitResult)
            },
            action = { tab, hitResult ->
                contextMenuUseCases.injectShareFromInternet(
                    tab.id,
                    ShareInternetResourceState(
                        url = hitResult.src,
                        private = tab.content.private,
                    ),
                )
            },
        )

        /**
         * Context Menu item: "Copy Link".
         *
         * @param context [Context] used for various system interactions.
         * @param snackBarParentView The view in which to find a suitable parent for displaying the `Snackbar`.
         * @param snackbarDelegate [SnackbarDelegate] used to actually show a `Snackbar`.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createCopyLinkCandidate(
            context: Context,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.copy_link",
            label = context.getString(R.string.mozac_feature_contextmenu_copy_link),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    (hitResult.isUri() || hitResult.isImage() || hitResult.isVideoAudio()) &&
                    additionalValidation(tab, hitResult)
            },
            action = { _, hitResult ->
                clipPlaintText(
                    context,
                    hitResult.getLink(),
                    hitResult.getLink(),
                    R.string.mozac_feature_contextmenu_snackbar_link_copied,
                    snackBarParentView,
                    snackbarDelegate,
                )
            },
        )

        /**
         * Context Menu item: "Copy Image Location".
         *
         * @param context [Context] used for various system interactions.
         * @param snackBarParentView The view in which to find a suitable parent for displaying the `Snackbar`.
         * @param snackbarDelegate [SnackbarDelegate] used to actually show a `Snackbar`.
         * @param additionalValidation Callback for the final validation in deciding whether this menu option
         * will be shown. Will only be called if all the intrinsic validations passed.
         */
        fun createCopyImageLocationCandidate(
            context: Context,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
            additionalValidation: (SessionState, HitResult) -> Boolean = { _, _ -> true },
        ) = ContextMenuCandidate(
            id = "mozac.feature.contextmenu.copy_image_location",
            label = context.getString(R.string.mozac_feature_contextmenu_copy_image_location),
            showFor = { tab, hitResult ->
                tab.isUrlSchemeAllowed(hitResult.getLink()) &&
                    hitResult.isImage() &&
                    additionalValidation(tab, hitResult)
            },
            action = { _, hitResult ->
                clipPlaintText(
                    context,
                    hitResult.getLink(),
                    hitResult.src,
                    R.string.mozac_feature_contextmenu_snackbar_link_copied,
                    snackBarParentView,
                    snackbarDelegate,
                )
            },
        )

        @Suppress("LongParameterList")
        private fun clipPlaintText(
            context: Context,
            label: String,
            plainText: String,
            displayTextId: Int,
            snackBarParentView: View,
            snackbarDelegate: SnackbarDelegate = DefaultSnackbarDelegate(),
        ) {
            val clipboardManager =
                context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText(label, plainText)
            clipboardManager.setPrimaryClip(clip)

            snackbarDelegate.show(
                snackBarParentView = snackBarParentView,
                text = displayTextId,
                duration = Snackbar.LENGTH_SHORT,
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
            listener: ((v: View) -> Unit)? = null,
        )
    }
}

// Some helper methods to work with HitResult. We may want to improve the API of HitResult and remove some of the
// helpers eventually: https://github.com/mozilla-mobile/android-components/issues/1443

private fun HitResult.isImage(): Boolean =
    (this is HitResult.IMAGE || this is HitResult.IMAGE_SRC) && src.isNotEmpty()

private fun HitResult.isVideoAudio(): Boolean =
    (this is HitResult.VIDEO || this is HitResult.AUDIO) && src.isNotEmpty()

private fun HitResult.isUri(): Boolean =
    ((this is HitResult.UNKNOWN && src.isNotEmpty()) || this is HitResult.IMAGE_SRC)

private fun HitResult.isHttpLink(): Boolean =
    isUri() && getLink().startsWith("http")

private fun HitResult.isLinkForOtherThanWebpage(): Boolean {
    val link = getLink()
    val isHtml = link.endsWith("html") || link.endsWith("htm")
    return isHttpLink() && !isHtml
}

private fun HitResult.isIntent(): Boolean =
    (
        this is HitResult.UNKNOWN && src.isNotEmpty() &&
            getLink().startsWith("intent:")
        )

private fun HitResult.isMailto(): Boolean =
    (this is HitResult.UNKNOWN && src.isNotEmpty()) &&
        getLink().startsWith("mailto:")

private fun HitResult.canOpenInExternalApp(appLinksUseCases: AppLinksUseCases): Boolean {
    if (isHttpLink() || isIntent() || isVideoAudio()) {
        val redirect = appLinksUseCases.appLinkRedirectIncludeInstall(getLink())
        return redirect.hasExternalApp() || redirect.hasMarketplaceIntent()
    }
    return false
}

internal fun HitResult.getLink(): String = when (this) {
    is HitResult.UNKNOWN -> src
    is HitResult.IMAGE_SRC -> uri
    is HitResult.IMAGE ->
        if (title.isNullOrBlank()) {
            src.takeOrReplace(MAX_TITLE_LENGTH, "image")
        } else {
            title.toString()
        }
    is HitResult.VIDEO ->
        if (title.isNullOrBlank()) src else title.toString()
    is HitResult.AUDIO ->
        if (title.isNullOrBlank()) src else title.toString()
    else -> "about:blank"
}

@VisibleForTesting
internal fun SessionState.isUrlSchemeAllowed(url: String): Boolean {
    return when (val engineSession = engineState.engineSession) {
        null -> true
        else -> {
            val urlScheme = Uri.parse(url).normalizeScheme().scheme
            !engineSession.getBlockedSchemes().contains(urlScheme)
        }
    }
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
        listener: ((v: View) -> Unit)?,
    ) {
        val snackbar = Snackbar.make(
            snackBarParentView,
            text,
            duration,
        )

        if (action != 0 && listener != null) {
            snackbar.setAction(action, listener)
        }

        snackbar.show()
    }
}
