/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.text.TextUtils
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.customtabs.createCustomTabConfigFromIntent
import mozilla.components.feature.customtabs.isCustomTabIntent
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.WebURLFinder
import org.mozilla.focus.activity.TextActionActivity
import org.mozilla.focus.ext.shouldRequestDesktopSite
import org.mozilla.focus.shortcut.HomeScreen
import org.mozilla.focus.utils.UrlUtils

/**
 * Implementation moved from Focus SessionManager. To be replaced with SessionIntentProcessor from feature-session
 * component soon.
 */
class IntentProcessor(
    private val context: Context,
    private val sessionManager: SessionManager
) {
    /**
     * Handle this incoming intent (via onCreate()) and create a new session if required.
     */
    fun handleIntent(context: Context, intent: SafeIntent, savedInstanceState: Bundle?): Session? {
        if ((intent.flags and Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0) {
            // This Intent was launched from history (recent apps). Android will redeliver the
            // original Intent (which might be a VIEW intent). However if there's no active browsing
            // session then we do not want to re-process the Intent and potentially re-open a website
            // from a session that the user already "erased".
            return null
        }

        if (savedInstanceState != null) {
            // We are restoring a previous session - No need to handle this Intent.
            return null
        }

        return createSessionFromIntent(context, intent)
    }

    /**
     * Handle this incoming intent (via onNewIntent()) and create a new session if required.
     */
    fun handleNewIntent(context: Context, intent: SafeIntent) {
        createSessionFromIntent(context, intent)
    }

    @Suppress("ComplexMethod", "ReturnCount")
    private fun createSessionFromIntent(context: Context, intent: SafeIntent): Session? {
        val action = intent.action

        when (action) {
            Intent.ACTION_VIEW -> {
                val dataString = intent.dataString
                if (TextUtils.isEmpty(dataString)) {
                    // If there's no URL in the Intent then we can't create a session.
                    return null
                }

                return when {
                    intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG) -> {
                        val blockingEnabled =
                            intent.getBooleanExtra(HomeScreen.BLOCKING_ENABLED, true)
                        val requestDesktop =
                            intent.getBooleanExtra(HomeScreen.REQUEST_DESKTOP, false)

                        createSession(
                            Session.Source.HOME_SCREEN,
                            intent,
                            intent.dataString ?: "",
                            blockingEnabled,
                            requestDesktop
                        )
                    }
                    intent.hasExtra(TextActionActivity.EXTRA_TEXT_SELECTION) -> createSession(
                        Session.Source.TEXT_SELECTION,
                        intent,
                        intent.dataString ?: ""
                    )
                    else -> createSession(
                        Session.Source.ACTION_VIEW,
                        intent,
                        intent.dataString ?: ""
                    )
                }
            }

            Intent.ACTION_SEND -> {
                val dataString = intent.getStringExtra(Intent.EXTRA_TEXT)
                if (TextUtils.isEmpty(dataString)) {
                    return null
                }

                return if (!UrlUtils.isUrl(dataString)) {
                    val bestURL = WebURLFinder(dataString).bestWebURL()
                    if (!TextUtils.isEmpty(bestURL)) {
                        createSession(Session.Source.ACTION_SEND, bestURL ?: "")
                    } else {
                        createSearchSession(
                            Session.Source.ACTION_SEND,
                            UrlUtils.createSearchUrl(context, dataString),
                            dataString ?: "")
                    }
                } else {
                    createSession(Session.Source.ACTION_SEND, dataString ?: "")
                }
            }

            else -> return null
        }
    }

    private fun createSession(source: Session.Source, url: String): Session {
        return Session(url, source = source).apply {
            sessionManager.add(this, selected = true)
        }
    }

    private fun createSearchSession(source: Session.Source, url: String, searchTerms: String): Session {
        return Session(url, source = source).apply {
            this.searchTerms = searchTerms
            sessionManager.add(this, selected = true)
        }
    }

    private fun createSession(source: Session.Source, intent: SafeIntent, url: String): Session {
        return if (isCustomTabIntent(intent.unsafe)) {
            Session(url, source = Session.Source.CUSTOM_TAB).apply {
                customTabConfig = createCustomTabConfigFromIntent(intent.unsafe, context.resources)
                sessionManager.add(this, selected = false)
            }
        } else {
            Session(url, source = source).apply {
                sessionManager.add(this, selected = true)
            }
        }
    }

    private fun createSession(
        source: Session.Source,
        intent: SafeIntent,
        url: String,
        blockingEnabled: Boolean,
        requestDesktop: Boolean
    ): Session {
        val session = if (isCustomTabIntent(intent)) {
            Session(url, source = Session.Source.CUSTOM_TAB).apply {
                customTabConfig = createCustomTabConfigFromIntent(intent.unsafe, context.resources)
            }
        } else {
            Session(url, source = source)
        }
        session.trackerBlockingEnabled = blockingEnabled
        session.shouldRequestDesktopSite = requestDesktop

        sessionManager.add(session, selected = !session.isCustomTabSession())

        return session
    }
}
