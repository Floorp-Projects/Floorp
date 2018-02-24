/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test.util

import org.mozilla.geckoview.GeckoSession

class Callbacks private constructor() {
    object Default : All {
    }

    interface All : ContentListener, NavigationListener, PermissionDelegate, ProgressListener,
                    PromptDelegate, ScrollListener, TrackingProtectionDelegate {
    }

    interface ContentListener : GeckoSession.ContentListener {
        override fun onTitleChange(session: GeckoSession, title: String) {
        }

        override fun onFocusRequest(session: GeckoSession) {
        }

        override fun onCloseRequest(session: GeckoSession) {
        }

        override fun onFullScreen(session: GeckoSession, fullScreen: Boolean) {
        }

        override fun onContextMenu(session: GeckoSession, screenX: Int, screenY: Int, uri: String, elementSrc: String) {
        }
    }

    interface NavigationListener : GeckoSession.NavigationListener {
        override fun onLocationChange(session: GeckoSession, url: String) {
        }

        override fun onCanGoBack(session: GeckoSession, canGoBack: Boolean) {
        }

        override fun onCanGoForward(session: GeckoSession, canGoForward: Boolean) {
        }

        override fun onLoadUri(session: GeckoSession, uri: String, where: GeckoSession.NavigationListener.TargetWindow): Boolean {
            return false;
        }

        override fun onNewSession(session: GeckoSession, uri: String, response: GeckoSession.Response<GeckoSession>) {
            response.respond(null)
        }
    }

    interface PermissionDelegate : GeckoSession.PermissionDelegate {
        override fun requestAndroidPermissions(session: GeckoSession, permissions: Array<out String>, callback: GeckoSession.PermissionDelegate.Callback) {
            callback.reject()
        }

        override fun requestContentPermission(session: GeckoSession, uri: String, type: String, access: String, callback: GeckoSession.PermissionDelegate.Callback) {
            callback.reject()
        }

        override fun requestMediaPermission(session: GeckoSession, uri: String, video: Array<out GeckoSession.PermissionDelegate.MediaSource>, audio: Array<out GeckoSession.PermissionDelegate.MediaSource>, callback: GeckoSession.PermissionDelegate.MediaCallback) {
            callback.reject()
        }
    }

    interface ProgressListener : GeckoSession.ProgressListener {
        override fun onPageStart(session: GeckoSession, url: String) {
        }

        override fun onPageStop(session: GeckoSession, success: Boolean) {
        }

        override fun onSecurityChange(session: GeckoSession, securityInfo: GeckoSession.ProgressListener.SecurityInformation) {
        }
    }

    interface PromptDelegate : GeckoSession.PromptDelegate {
        override fun alert(session: GeckoSession, title: String, msg: String, callback: GeckoSession.PromptDelegate.AlertCallback) {
            callback.dismiss()
        }

        override fun promptForButton(session: GeckoSession, title: String, msg: String, btnMsg: Array<out String>, callback: GeckoSession.PromptDelegate.ButtonCallback) {
            callback.dismiss()
        }

        override fun promptForText(session: GeckoSession, title: String, msg: String, value: String, callback: GeckoSession.PromptDelegate.TextCallback) {
            callback.dismiss()
        }

        override fun promptForAuth(session: GeckoSession, title: String, msg: String, options: GeckoSession.PromptDelegate.AuthenticationOptions, callback: GeckoSession.PromptDelegate.AuthCallback) {
            callback.dismiss()
        }

        override fun promptForChoice(session: GeckoSession, title: String, msg: String, type: Int, choices: Array<out GeckoSession.PromptDelegate.Choice>, callback: GeckoSession.PromptDelegate.ChoiceCallback) {
            callback.dismiss()
        }

        override fun promptForColor(session: GeckoSession, title: String, value: String, callback: GeckoSession.PromptDelegate.TextCallback) {
            callback.dismiss()
        }

        override fun promptForDateTime(session: GeckoSession, title: String, type: Int, value: String, min: String, max: String, callback: GeckoSession.PromptDelegate.TextCallback) {
            callback.dismiss()
        }

        override fun promptForFile(session: GeckoSession, title: String, type: Int, mimeTypes: Array<out String>, callback: GeckoSession.PromptDelegate.FileCallback) {
            callback.dismiss()
        }
    }

    interface ScrollListener : GeckoSession.ScrollListener {
        override fun onScrollChanged(session: GeckoSession, scrollX: Int, scrollY: Int) {
        }
    }

    interface TrackingProtectionDelegate : GeckoSession.TrackingProtectionDelegate {
        override fun onTrackerBlocked(session: GeckoSession, uri: String, categories: Int) {
        }
    }
}
