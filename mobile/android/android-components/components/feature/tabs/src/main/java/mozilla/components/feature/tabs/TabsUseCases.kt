/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.browser.session.storage.RecoverableBrowserState
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.RestoreCompleteAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.findTabByUrl
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.toTabSessionStates
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.feature.session.SessionUseCases.LoadUrlUseCase

/**
 * Contains use cases related to the tabs feature.
 */
class TabsUseCases(
    store: BrowserStore
) {
    /**
     * Contract for use cases that select a tab.
     */
    interface SelectTabUseCase {
        /**
         * Select tab with the given [tabId].
         */
        operator fun invoke(tabId: String)
    }

    class DefaultSelectTabUseCase internal constructor(
        private val store: BrowserStore
    ) : SelectTabUseCase {
        /**
         * Marks the tab with the provided [tabId] as selected.
         */
        override fun invoke(tabId: String) {
            store.dispatch(TabListAction.SelectTabAction(tabId))
        }
    }

    /**
     * Contract for use cases that remove a tab.
     */
    interface RemoveTabUseCase {
        /**
         * Removes the session with the provided ID. This method
         * has no effect if the session doesn't exist.
         *
         * @param tabId The ID of the session to remove.
         */
        operator fun invoke(tabId: String)

        /**
         * Removes the session with the provided ID. This method
         * has no effect if the session doesn't exist.
         *
         * @param tabId The ID of the session to remove.
         * @param selectParentIfExists Whether or not to select the parent tab
         * of the removed tab if a parent exists. Note that the default implementation
         * of this method will ignore [selectParentIfExists] and never select a parent.
         * This is a temporary workaround to prevent additional API breakage for
         * subtypes other than [DefaultRemoveTabUseCase]. The default implementation
         * should be removed together with invoke(Session).
         */
        operator fun invoke(tabId: String, selectParentIfExists: Boolean) = invoke(tabId)
    }

    /**
     * Default implementation of [RemoveTabUseCase].
     */
    class DefaultRemoveTabUseCase internal constructor(
        private val store: BrowserStore
    ) : RemoveTabUseCase {

        /**
         * Removes the tab with the provided ID. This method
         * has no effect if the tab doesn't exist.
         *
         * @param tabId The ID of the tab to remove.
         */
        override operator fun invoke(tabId: String) {
            store.dispatch(TabListAction.RemoveTabAction(tabId))
        }

        /**
         * Removes the session with the provided ID. This method
         * has no effect if the session doesn't exist.
         *
         * @param tabId The ID of the session to remove.
         * @param selectParentIfExists Whether or not to select the parent tab
         * of the removed tab if a parent exists.
         */
        override operator fun invoke(tabId: String, selectParentIfExists: Boolean) {
            store.dispatch(TabListAction.RemoveTabAction(tabId, selectParentIfExists))
        }
    }

    class AddNewTabUseCase internal constructor(
        private val store: BrowserStore
    ) : LoadUrlUseCase {

        /**
         * Adds a new tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         */
        override fun invoke(url: String, flags: LoadUrlFlags, additionalHeaders: Map<String, String>?) {
            this.invoke(url, selectTab = true, startLoading = true, parentId = null, flags = flags)
        }

        /**
         * Adds a new tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param selectTab True (default) if the new tab should be selected immediately.
         * @param startLoading True (default) if the new tab should start loading immediately.
         * @param parentId the id of the parent tab to use for the newly created tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         * @param contextId the session context id to use for this tab.
         * @param engineSession (optional) engine session to use for this tab.
         * @return The ID of the created tab.
         */
        @Suppress("LongParameterList")
        operator fun invoke(
            url: String = "about:blank",
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parentId: String? = null,
            flags: LoadUrlFlags = LoadUrlFlags.none(),
            contextId: String? = null,
            engineSession: EngineSession? = null,
            source: SessionState.Source = SessionState.Source.Internal.NewTab,
            searchTerms: String = "",
            private: Boolean = false
        ): String {
            val tab = createTab(
                url = url,
                private = private,
                source = source,
                contextId = contextId,
                parent = parentId?.let { store.state.findTab(it) },
                engineSession = engineSession,
                searchTerms = searchTerms,
                initialLoadFlags = flags
            )

            store.dispatch(TabListAction.AddTabAction(tab, select = selectTab))

            // If an engine session is specified then loading will have already started when linking
            // the tab to its engine session. Otherwise we ask to load the URL here.
            if (startLoading && engineSession == null) {
                store.dispatch(
                    EngineAction.LoadUrlAction(
                        tab.id,
                        url,
                        flags
                    )
                )
            }

            return tab.id
        }
    }

    @Deprecated("Use AddNewTabUseCase and the private flag")
    class AddNewPrivateTabUseCase internal constructor(
        private val store: BrowserStore
    ) : LoadUrlUseCase {

        /**
         * Adds a new private tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new private tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         */
        override fun invoke(url: String, flags: LoadUrlFlags, additionalHeaders: Map<String, String>?) {
            this.invoke(url, selectTab = true, startLoading = true, parentId = null, flags = flags)
        }

        /**
         * Adds a new private tab and loads the provided URL.
         *
         * @param url The URL to be loaded in the new tab.
         * @param selectTab True (default) if the new tab should be selected immediately.
         * @param startLoading True (default) if the new tab should start loading immediately.
         * @param parentId the id of the parent tab to use for the newly created tab.
         * @param flags the [LoadUrlFlags] to use when loading the provided URL.
         * @param engineSession (optional) engine session to use for this tab.
         * @param searchTerms (optional) search terms that were used to create this tab.
         * @return The ID of the created tab.
         */
        @Suppress("LongParameterList")
        operator fun invoke(
            url: String = "about:blank",
            selectTab: Boolean = true,
            startLoading: Boolean = true,
            parentId: String? = null,
            flags: LoadUrlFlags = LoadUrlFlags.none(),
            engineSession: EngineSession? = null,
            source: SessionState.Source = SessionState.Source.Internal.NewTab,
            searchTerms: String? = null
        ): String {
            val tab = createTab(
                url = url,
                private = true,
                source = source,
                parent = parentId?.let { store.state.findTab(it) },
                engineSession = engineSession,
                searchTerms = searchTerms ?: "",
                initialLoadFlags = flags
            )

            store.dispatch(TabListAction.AddTabAction(tab, select = selectTab))

            // If an engine session is specified then loading will have already started when linking
            // the tab to its engine session. Otherwise we ask to load the URL here.
            if (startLoading && engineSession == null) {
                store.dispatch(
                    EngineAction.LoadUrlAction(
                        tab.id,
                        url,
                        flags
                    )
                )
            }

            return tab.id
        }
    }

    /**
     * Use case for removing a list of tabs.
     */
    class RemoveTabsUseCase internal constructor(
        private val store: BrowserStore
    ) {
        /**
         * Removes a specified list of tabs.
         */
        operator fun invoke(ids: List<String>) {
            store.dispatch(TabListAction.RemoveTabsAction(ids))
        }
    }

    class RemoveAllTabsUseCase internal constructor(
        private val store: BrowserStore
    ) {
        /**
         * Removes all tabs.
         * @param recoverable Indicates whether removed tabs should be recoverable.
         */
        operator fun invoke(recoverable: Boolean = true) {
            store.dispatch(TabListAction.RemoveAllTabsAction(recoverable))
        }
    }

    /**
     * Use case for removing all normal (non-private) tabs.
     */
    class RemoveNormalTabsUseCase internal constructor(
        private val store: BrowserStore
    ) {
        /**
         * Removes all normal (non-private) tabs.
         */
        operator fun invoke() {
            store.dispatch(TabListAction.RemoveAllNormalTabsAction)
        }
    }

    /**
     * Use case for removing all private tabs.
     */
    class RemovePrivateTabsUseCase internal constructor(
        private val store: BrowserStore
    ) {
        /**
         * Removes all private tabs.
         */
        operator fun invoke() {
            store.dispatch(TabListAction.RemoveAllPrivateTabsAction)
        }
    }

    /**
     * Use case for restoring removed tabs ("undo").
     */
    class UndoTabRemovalUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Restores the list of tabs in the undo history.
         */
        operator fun invoke() {
            store.dispatch(UndoAction.RestoreRecoverableTabs)
        }
    }

    /**
     * Use case for restoring tabs.
     */
    class RestoreUseCase(
        private val store: BrowserStore,
        private val selectTab: SelectTabUseCase
    ) {
        /**
         * Restores the given list of [RecoverableTab]s.
         */
        operator fun invoke(tabs: List<RecoverableTab>, selectTabId: String? = null) {
            store.dispatch(
                TabListAction.RestoreAction(tabs.toTabSessionStates(), selectTabId)
            )
        }

        /**
         * Restores the given [RecoverableBrowserState].
         */
        operator fun invoke(state: RecoverableBrowserState) {
            invoke(state.tabs, state.selectedTabId)
        }

        /**
         * Restores the browsing session from the given [SessionStorage]. Also dispatches
         * [RestoreCompleteAction] on the [BrowserStore] once restore has been completed.
         *
         * @param storage the [SessionStorage] to restore state from.
         * @param tabTimeoutInMs the amount of time in milliseconds after which inactive
         * tabs will be discarded and not restored. Defaults to Long.MAX_VALUE, meaning
         * all tabs will be restored by default.
         */
        suspend operator fun invoke(
            storage: SessionStorage,
            tabTimeoutInMs: Long = Long.MAX_VALUE
        ) = withContext(Dispatchers.IO) {
            val now = System.currentTimeMillis()
            val state = storage.restore {
                val lastActiveTime = maxOf(it.lastAccess, it.createdAt)
                now - lastActiveTime <= tabTimeoutInMs
            }
            if (state != null) {
                withContext(Dispatchers.Main) {
                    invoke(state)
                }
            }
            store.dispatch(RestoreCompleteAction)
        }

        /**
         * Restores the given [RecoverableTab] and updates the selected tab if [updateSelection] is
         * `true`.
         */
        operator fun invoke(tab: RecoverableTab, updateSelection: Boolean = true) {
            invoke(listOf(tab))

            if (updateSelection) {
                selectTab(tab.id)
            }
        }
    }

    /**
     * Use case for selecting an existing tab or creating a new tab with a specific URL.
     */
    class SelectOrAddUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Selects an already existing tab with the matching [HistoryMetadataKey] or otherwise
         * creates a new tab with the given [url].
         *
         * @param url The URL to be selected or loaded in the new tab.
         * @param historyMetadata The [HistoryMetadataKey] to match for existing tabs.
         * @return The ID of the selected or created tab.
         */
        operator fun invoke(
            url: String,
            historyMetadata: HistoryMetadataKey
        ): String {
            val tab = store.state.tabs.find { it.historyMetadata == historyMetadata }

            return if (tab != null) {
                store.dispatch(TabListAction.SelectTabAction(tab.id))
                tab.id
            } else {
                this.invoke(url)
            }
        }

        /**
         * Selects an already existing tab displaying [url] or otherwise creates a new tab.
         *
         * @param url The URL to be loaded in the new tab.
         * @param private Whether or not this session should use private mode.
         * @param source The origin of a session to describe how and why it was created.
         * @param flags The [LoadUrlFlags] to use when loading the provided URL.
         * @return The ID of the selected or created tab.
         */
        operator fun invoke(
            url: String,
            private: Boolean = false,
            source: SessionState.Source = SessionState.Source.Internal.NewTab,
            flags: LoadUrlFlags = LoadUrlFlags.none()
        ): String {
            val existingTab = store.state.findTabByUrl(url)

            return if (existingTab != null) {
                store.dispatch(TabListAction.SelectTabAction(existingTab.id))
                existingTab.id
            } else {
                val tab = createTab(
                    url = url,
                    private = private,
                    source = source,
                    initialLoadFlags = flags
                )
                store.dispatch(TabListAction.AddTabAction(tab, select = true))
                store.dispatch(
                    EngineAction.LoadUrlAction(
                        tab.id,
                        url,
                        flags
                    )
                )
                tab.id
            }
        }
    }

    /**
     * Use case for duplicating a tab.
     */
    class DuplicateTabUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Creates a duplicate of [tab] (including history) and selects it if [selectNewTab] is true.
         */
        operator fun invoke(
            tab: TabSessionState,
            selectNewTab: Boolean = true
        ) {
            val duplicate = createTab(
                url = tab.content.url,
                private = tab.content.private,
                contextId = tab.contextId,
                parent = tab,
                engineSessionState = tab.engineState.engineSessionState
            )

            store.dispatch(
                TabListAction.AddTabAction(
                    duplicate,
                    select = selectNewTab
                )
            )
        }
    }

    val selectTab: SelectTabUseCase by lazy { DefaultSelectTabUseCase(store) }
    val removeTab: RemoveTabUseCase by lazy { DefaultRemoveTabUseCase(store) }
    val addTab: AddNewTabUseCase by lazy { AddNewTabUseCase(store) }
    @Deprecated("Use addTab and the private flag")
    @Suppress("DEPRECATION")
    val addPrivateTab: AddNewPrivateTabUseCase by lazy { AddNewPrivateTabUseCase(store) }
    val removeAllTabs: RemoveAllTabsUseCase by lazy { RemoveAllTabsUseCase(store) }
    val removeTabs: RemoveTabsUseCase by lazy { RemoveTabsUseCase(store) }
    val removeNormalTabs: RemoveNormalTabsUseCase by lazy { RemoveNormalTabsUseCase(store) }
    val removePrivateTabs: RemovePrivateTabsUseCase by lazy { RemovePrivateTabsUseCase(store) }
    val undo by lazy { UndoTabRemovalUseCase(store) }
    val restore: RestoreUseCase by lazy { RestoreUseCase(store, selectTab) }
    val selectOrAddTab: SelectOrAddUseCase by lazy { SelectOrAddUseCase(store) }
    val duplicateTab: DuplicateTabUseCase by lazy { DuplicateTabUseCase(store) }
}
