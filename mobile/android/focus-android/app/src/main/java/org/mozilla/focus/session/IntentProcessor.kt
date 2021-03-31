/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.text.TextUtils
import mozilla.components.browser.state.state.SessionState
import mozilla.components.feature.customtabs.createCustomTabConfigFromIntent
import mozilla.components.feature.customtabs.isCustomTabIntent
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.utils.SafeIntent
import mozilla.components.support.utils.WebURLFinder
import org.mozilla.focus.activity.TextActionActivity
import org.mozilla.focus.ext.components
import org.mozilla.focus.shortcut.HomeScreen
import org.mozilla.focus.utils.SearchUtils
import org.mozilla.focus.utils.UrlUtils

/**
 * Implementation moved from Focus SessionManager. To be replaced with SessionIntentProcessor from feature-session
 * component soon.
 */
class IntentProcessor(
    private val context: Context,
    private val tabsUseCases: TabsUseCases,
    private val customTabsUseCases: CustomTabsUseCases
) {
    sealed class Result {
        object None : Result()
        data class Tab(val id: String) : Result()
        data class CustomTab(val id: String) : Result()
    }

    /**
     * Handle this incoming intent (via onCreate()) and create a new session if required.
     */
    fun handleIntent(context: Context, intent: SafeIntent, savedInstanceState: Bundle?): Result {
        if ((intent.flags and Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0) {
            // This Intent was launched from history (recent apps). Android will redeliver the
            // original Intent (which might be a VIEW intent). However if there's no active browsing
            // session then we do not want to re-process the Intent and potentially re-open a website
            // from a session that the user already "erased".
            return Result.None
        }

        if (savedInstanceState != null) {
            // We are restoring a previous session - No need to handle this Intent.
            return Result.None
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
    private fun createSessionFromIntent(context: Context, intent: SafeIntent): Result {
        val action = intent.action

        when (action) {
            Intent.ACTION_VIEW -> {
                val dataString = intent.dataString
                if (TextUtils.isEmpty(dataString)) {
                    // If there's no URL in the Intent then we can't create a session.
                    return Result.None
                }

                return when {
                    intent.hasExtra(HomeScreen.ADD_TO_HOMESCREEN_TAG) -> {
                        val requestDesktop =
                            intent.getBooleanExtra(HomeScreen.REQUEST_DESKTOP, false)

                        // Ignoring, because exception!
                        // HomeScreen.BLOCKING_ENABLED

                        createSession(
                            SessionState.Source.HOME_SCREEN,
                            intent,
                            intent.dataString ?: "",
                            requestDesktop
                        )
                    }
                    intent.hasExtra(TextActionActivity.EXTRA_TEXT_SELECTION) -> createSession(
                        SessionState.Source.TEXT_SELECTION,
                        intent,
                        intent.dataString ?: ""
                    )
                    else -> createSession(
                        SessionState.Source.ACTION_VIEW,
                        intent,
                        intent.dataString ?: ""
                    )
                }
            }

            Intent.ACTION_SEND -> {
                val dataString = intent.getStringExtra(Intent.EXTRA_TEXT)
                if (TextUtils.isEmpty(dataString)) {
                    return Result.None
                }

                return if (!UrlUtils.isUrl(dataString)) {
                    val bestURL = WebURLFinder(dataString).bestWebURL()
                    if (!TextUtils.isEmpty(bestURL)) {
                        createSession(SessionState.Source.ACTION_SEND, bestURL ?: "")
                    } else {
                        createSearchSession(
                            SessionState.Source.ACTION_SEND,
                            SearchUtils.createSearchUrl(context, dataString ?: ""),
                            dataString ?: "")
                    }
                } else {
                    createSession(SessionState.Source.ACTION_SEND, dataString ?: "")
                }
            }

            else -> return Result.None
        }
    }

    private fun createSession(source: SessionState.Source, url: String): Result {
        return Result.Tab(tabsUseCases.addPrivateTab(
            url,
            source = source,
            selectTab = true
        ))
    }

    private fun createSearchSession(source: SessionState.Source, url: String, searchTerms: String): Result {
        return Result.Tab(tabsUseCases.addPrivateTab(
            url,
            source = source,
            searchTerms = searchTerms
        ))
    }

    private fun createSession(source: SessionState.Source, intent: SafeIntent, url: String): Result {
        return if (isCustomTabIntent(intent.unsafe)) {
            Result.CustomTab(customTabsUseCases.add(
                url,
                createCustomTabConfigFromIntent(intent.unsafe, context.resources),
                private = true
            ))
        } else {
            Result.Tab(tabsUseCases.addPrivateTab(
                url,
                source = source,
                selectTab = true
            ))
        }
    }

    private fun createSession(
        source: SessionState.Source,
        intent: SafeIntent,
        url: String,
        requestDesktop: Boolean
    ): Result {
        val (result, tabId) = if (isCustomTabIntent(intent)) {
            val tabId = customTabsUseCases.add(
                url,
                createCustomTabConfigFromIntent(intent.unsafe, context.resources),
                private = true
            )
            Pair(Result.CustomTab(tabId), tabId)
        } else {
            val tabId = tabsUseCases.addPrivateTab(
                url,
                source = source
            )
            Pair(Result.Tab(tabId), tabId)
        }

        if (requestDesktop) {
            context.components.sessionUseCases.requestDesktopSite(requestDesktop, tabId)
        }

        return result
    }
}
