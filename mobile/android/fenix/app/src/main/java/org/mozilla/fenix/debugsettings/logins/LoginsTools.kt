/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.logins

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.EncryptedLogin
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginEntry
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.lib.state.ext.observeAsComposableState
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.debugsettings.ui.DebugDrawer
import org.mozilla.fenix.theme.FirefoxTheme
import java.util.UUID

/**
 * Logins UI for [DebugDrawer] that displays existing logins for the current domain and allows
 * deleting logins or adding new fake logins.
 *
 * @param browserStore [BrowserStore] used to access the selected tab.
 * @param loginsStorage [LoginsStorage] used to access logins.
 */
@Composable
fun LoginsTools(
    browserStore: BrowserStore,
    loginsStorage: LoginsStorage,
) {
    val scope = rememberCoroutineScope()
    var existingLogins by remember { mutableStateOf(listOf<Login>()) }
    val origin by browserStore.observeAsComposableState { browserState ->
        browserState.selectedTabId?.let { id ->
            browserStore.state.findTab(id)?.content?.url?.tryGetHostFromUrl()
        }
    }

    LaunchedEffect(origin) {
        origin?.let {
            existingLogins = loginsStorage.getByBaseDomain(it)
        }
    }

    LoginsContent(
        origin = origin,
        existingLogins = existingLogins,
        onAddFakeLogin = {
            origin?.let {
                scope.launch {
                    val new = loginsStorage.add(
                        LoginEntry(
                            username = "fake_username${existingLogins.size + 1}",
                            password = "fake_password${existingLogins.size + 1}",
                            origin = "https://$origin",
                            formActionOrigin = "https://$origin",
                        ),
                    )
                    existingLogins += loginsStorage.decryptLogin(new)
                }
            }
        },
        onDeleteLogin = { entry ->
            scope.launch {
                loginsStorage.delete(entry.guid).also { isSuccess ->
                    if (isSuccess) {
                        existingLogins -= entry
                    }
                }
            }
        },
    )
}

@Composable
private fun LoginsContent(
    origin: String?,
    existingLogins: List<Login>,
    onAddFakeLogin: () -> Unit,
    onDeleteLogin: (Login) -> Unit,
) {
    Column(modifier = Modifier.padding(16.dp)) {
        Text(
            text = stringResource(R.string.debug_drawer_logins_title),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline5,
        )

        Text(
            color = FirefoxTheme.colors.textSecondary,
            text = stringResource(
                R.string.debug_drawer_logins_current_domain_label,
                origin ?: "",
            ),
        )

        Spacer(modifier = Modifier.height(16.dp))

        PrimaryButton(
            text = stringResource(R.string.debug_drawer_logins_add_login_button),
            onClick = onAddFakeLogin,
        )

        LazyColumn {
            items(existingLogins) { login ->
                TextListItem(
                    label = login.username,
                    onIconClick = { onDeleteLogin(login) },
                    iconPainter = painterResource(R.drawable.ic_delete),
                    iconDescription = stringResource(
                        R.string.debug_drawer_logins_delete_login_button_content_description,
                        login.username,
                    ),
                )
            }
        }
    }
}

@Composable
@LightDarkPreview
private fun LoginsScreenPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
        ) {
            val selectedTab = createTab("https://example.com")
            LoginsTools(
                browserStore = BrowserStore(
                    BrowserState(
                        selectedTabId = selectedTab.id,
                        tabs = listOf(selectedTab),
                    ),
                ),
                loginsStorage = FakeLoginsStorage(),
            )
        }
    }
}

internal class FakeLoginsStorage : LoginsStorage {
    private val loginsByGuid = mutableMapOf<String, LoginEntry>()
    override suspend fun wipeLocal() = Unit
    override suspend fun delete(guid: String): Boolean = loginsByGuid.remove(guid) != null
    override suspend fun get(guid: String): Login? = null
    override suspend fun touch(guid: String) = Unit
    override suspend fun list(): List<Login> = listOf()
    override suspend fun findLoginToUpdate(entry: LoginEntry): Login? = null
    override suspend fun add(entry: LoginEntry): EncryptedLogin {
        val guid = UUID.randomUUID().toString()
        loginsByGuid[guid] = entry
        return EncryptedLogin(guid, "", secFields = "")
    }
    override suspend fun update(guid: String, entry: LoginEntry) =
        EncryptedLogin("", "", secFields = "")
    override suspend fun addOrUpdate(entry: LoginEntry) =
        EncryptedLogin("", "", secFields = "")
    override suspend fun getByBaseDomain(origin: String) = loginsByGuid.map { (guid, login) ->
        Login(
            guid = guid,
            origin = login.origin,
            username = login.username,
            password = login.password,
        )
    }
    override suspend fun decryptLogin(login: EncryptedLogin): Login =
        with(loginsByGuid[login.guid]!!) {
            Login(
                guid = login.guid,
                origin = origin,
                username = username,
                password = password,
            )
        }
    override fun close() = Unit
}
