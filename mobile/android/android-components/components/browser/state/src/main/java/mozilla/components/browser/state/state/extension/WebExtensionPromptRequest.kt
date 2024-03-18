/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state.extension

import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.concept.engine.webextension.WebExtensionInstallException

/**
 * Value type that represents a request for showing a native dialog from a [WebExtension].
 *
 * @param extension The [WebExtension] that requested the dialog to be shown.
 */
sealed class WebExtensionPromptRequest {

    /**
     * Value type that represents a request for showing a native dialog from a [WebExtension] before
     * the installation succeeds.
     */
    sealed class BeforeInstallation(open val extension: WebExtension?) :
        WebExtensionPromptRequest() {
        /**
         * Value type that represents a request for showing error prompt when an installation failed.
         * @property extension The exception with failed to installed.
         * @property exception The reason why the installation failed.
         */
        data class InstallationFailed(
            override val extension: WebExtension?,
            val exception: WebExtensionInstallException,
        ) : BeforeInstallation(extension)
    }

    /**
     * Value type that represents a request for showing a native dialog from a [WebExtension] after
     * installation succeeds.
     *
     * @param extension The [WebExtension] that requested the dialog to be shown.
     */
    sealed class AfterInstallation(open val extension: WebExtension) : WebExtensionPromptRequest() {
        /**
         * Value type that represents a request for showing a permissions prompt.
         */
        sealed class Permissions(
            override val extension: WebExtension,
            open val onConfirm: (Boolean) -> Unit,
        ) : AfterInstallation(extension) {
            /**
             * Value type that represents a request for a required permissions prompt.
             * @property extension The [WebExtension] that requested the dialog to be shown.
             * @property onConfirm A callback indicating whether the permissions were granted or not.
             */
            data class Required(
                override val extension: WebExtension,
                override val onConfirm: (Boolean) -> Unit,
            ) : Permissions(extension, onConfirm)

            /**
             * Value type that represents a request for an optional permissions prompt.
             * @property extension The [WebExtension] that requested the dialog to be shown.
             * @property permissions The optional permissions to list in the dialog.
             * @property onConfirm A callback indicating whether the permissions were granted or not.
             */
            data class Optional(
                override val extension: WebExtension,
                val permissions: List<String>,
                override val onConfirm: (Boolean) -> Unit,
            ) : Permissions(extension, onConfirm)
        }

        /**
         * Value type that represents a request for showing post-installation prompt.
         * Normally used to give users an opportunity to enable the [extension] in private browsing mode.
         * @property extension The installed extension.
         */
        data class PostInstallation(
            override val extension: WebExtension,
        ) : AfterInstallation(extension)
    }
}
