/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.extension

import mozilla.components.concept.engine.webextension.WebExtension

/**
 * Value type that represents a request for showing a native dialog from a [WebExtension].
 *
 * @param extension The [WebExtension] that requested the dialog to be shown.
 */
sealed class WebExtensionPromptRequest(
    open val extension: WebExtension,
) {

    /**
     * Value type that represents a request for a permission prompt.
     * @property extension The [WebExtension] that requested the dialog to be shown.
     * @property onConfirm A callback indicating whether the permissions were granted or not.
     */
    data class Permissions(
        override val extension: WebExtension,
        val onConfirm: (Boolean) -> Unit,
    ) : WebExtensionPromptRequest(extension)

    /**
     * Value type that represents a request for showing post-installation prompt.
     * Normally used to give users an opportunity to enable the [extension] in private browsing mode.
     * @property extension The installed extension.
     */
    data class PostInstallation(
        override val extension: WebExtension,
    ) : WebExtensionPromptRequest(extension)
}
