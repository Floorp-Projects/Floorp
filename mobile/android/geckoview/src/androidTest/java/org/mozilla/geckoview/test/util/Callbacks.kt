/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test.util

import org.mozilla.geckoview.AllowOrDeny
import org.mozilla.geckoview.ContentBlocking
import org.mozilla.geckoview.GeckoResponse
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.GeckoSession
import org.mozilla.geckoview.GeckoSession.ContentDelegate.ContextElement
import org.mozilla.geckoview.GeckoSession.NavigationDelegate.LoadRequest
import org.mozilla.geckoview.MediaElement
import org.mozilla.geckoview.WebRequestError

import android.view.inputmethod.CursorAnchorInfo
import android.view.inputmethod.ExtractedText
import android.view.inputmethod.ExtractedTextRequest
import org.json.JSONObject

class Callbacks private constructor() {
    object Default : All

    interface All : ContentBlockingDelegate, ContentDelegate,
                    HistoryDelegate, MediaDelegate,
                    NavigationDelegate, PermissionDelegate, ProgressDelegate,
                    PromptDelegate, ScrollDelegate, SelectionActionDelegate,
                    TextInputDelegate

    interface ContentDelegate : GeckoSession.ContentDelegate {}
    interface NavigationDelegate : GeckoSession.NavigationDelegate {}
    interface PermissionDelegate : GeckoSession.PermissionDelegate {}
    interface ProgressDelegate : GeckoSession.ProgressDelegate {}
    interface PromptDelegate : GeckoSession.PromptDelegate {}
    interface ScrollDelegate : GeckoSession.ScrollDelegate {}
    interface ContentBlockingDelegate : ContentBlocking.Delegate {}
    interface SelectionActionDelegate : GeckoSession.SelectionActionDelegate {}
    interface MediaDelegate: GeckoSession.MediaDelegate {}
    interface HistoryDelegate : GeckoSession.HistoryDelegate {}

    interface TextInputDelegate : GeckoSession.TextInputDelegate {
        override fun restartInput(session: GeckoSession, reason: Int) {
        }

        override fun showSoftInput(session: GeckoSession) {
        }

        override fun hideSoftInput(session: GeckoSession) {
        }

        override fun updateSelection(session: GeckoSession, selStart: Int, selEnd: Int, compositionStart: Int, compositionEnd: Int) {
        }

        override fun updateExtractedText(session: GeckoSession, request: ExtractedTextRequest, text: ExtractedText) {
        }

        override fun updateCursorAnchorInfo(session: GeckoSession, info: CursorAnchorInfo) {
        }

        override fun notifyAutoFill(session: GeckoSession, notification: Int, virtualId: Int) {
        }
    }
}
