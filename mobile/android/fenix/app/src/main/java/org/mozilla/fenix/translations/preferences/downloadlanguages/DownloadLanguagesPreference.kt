/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.downloadlanguages

import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import mozilla.components.feature.downloads.toMegabyteOrKilobyteString
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.translations.DownloadIconIndicator
import org.mozilla.geckoview.TranslationsController
import java.util.Locale

/**
 * Firefox Download Languages preference screen.
 *
 * @param downloadLanguageItemPreferences List of [DownloadLanguageItemPreference]s that needs to be displayed.
 * @param onItemClick Invoked when the user clicks on the language item.
 */
@Suppress("LongMethod")
@Composable
fun DownloadLanguagesPreference(
    downloadLanguageItemPreferences: List<DownloadLanguageItemPreference>,
    onItemClick: (DownloadLanguageItemPreference) -> Unit,
) {
    val downloadedItems = downloadLanguageItemPreferences.filter {
        it.state.status == DownloadLanguageItemStatusPreference.Downloaded &&
            it.state.type == DownloadLanguageItemTypePreference.GeneralLanguage
    }

    val notDownloadedItems = downloadLanguageItemPreferences.filter {
        it.state.status == DownloadLanguageItemStatusPreference.NotDownloaded &&
            it.state.type == DownloadLanguageItemTypePreference.GeneralLanguage
    }

    val inProgressItems = downloadLanguageItemPreferences.filter {
        it.state.status == DownloadLanguageItemStatusPreference.DownloadInProgress &&
            it.state.type == DownloadLanguageItemTypePreference.GeneralLanguage
    }

    val allLanguagesItem = downloadLanguageItemPreferences.last {
        it.state.type == DownloadLanguageItemTypePreference.AllLanguages
    }

    val defaultLanguageItem = downloadLanguageItemPreferences.last {
        it.state.type == DownloadLanguageItemTypePreference.PivotLanguage
    }

    Column(
        modifier = Modifier.background(
            color = FirefoxTheme.colors.layer1,
        ),
    ) {
        DownloadLanguagesHeaderPreference()

        LazyColumn {
            item {
                DownloadLanguagesHeader(
                    stringResource(
                        id = R.string.download_languages_available_languages_preference,
                    ),
                )
            }

            item {
                LanguageItemPreference(
                    item = defaultLanguageItem,
                    onItemClick = onItemClick,
                )
            }

            items(downloadedItems) { item: DownloadLanguageItemPreference ->
                LanguageItemPreference(
                    item = item,
                    onItemClick = onItemClick,
                )
            }

            items(inProgressItems) { item: DownloadLanguageItemPreference ->
                LanguageItemPreference(
                    item = item,
                    onItemClick = onItemClick,
                )
            }

            item {
                Divider(Modifier.padding(top = 8.dp, bottom = 8.dp))
            }

            item {
                DownloadLanguagesHeader(
                    stringResource(
                        id = R.string.download_language_header_preference,
                    ),
                )
            }

            item {
                LanguageItemPreference(
                    item = allLanguagesItem,
                    onItemClick = onItemClick,
                )
                Divider(Modifier.padding(top = 8.dp, bottom = 8.dp))
            }

            items(notDownloadedItems) { item: DownloadLanguageItemPreference ->
                item.languageModel.language?.localizedDisplayName?.let {
                    LanguageItemPreference(
                        item = item,
                        onItemClick = onItemClick,
                    )
                }
            }
        }
    }
}

@Composable
private fun DownloadLanguagesHeader(title: String) {
    Text(
        text = title,
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 72.dp, end = 16.dp, top = 8.dp)
            .semantics { heading() },
        color = FirefoxTheme.colors.textAccent,
        style = FirefoxTheme.typography.headline8,
    )
}

@Composable
private fun LanguageItemPreference(
    item: DownloadLanguageItemPreference,
    onItemClick: (DownloadLanguageItemPreference) -> Unit,
) {
    val description: String =
        if (item.state.type == DownloadLanguageItemTypePreference.PivotLanguage) {
            "(" + stringResource(id = R.string.download_languages_default_system_language_require_preference) + ")"
        } else {
            "(" + item.languageModel.size.toMegabyteOrKilobyteString() + ")"
        }

    val contentDescription =
        downloadLanguageItemContentDescriptionPreference(item = item, itemDescription = description)

    item.languageModel.language?.localizedDisplayName?.let {
        TextListItemInlineDescription(
            label = it,
            modifier = Modifier
                .padding(
                    start = 56.dp,
                )
                .clearAndSetSemantics {
                    role = Role.Button
                    this.contentDescription =
                        contentDescription
                },
            description = description,
            enabled = item.state.status != DownloadLanguageItemStatusPreference.DownloadInProgress,
            onClick = { onItemClick(item) },
            icon = { IconDownloadLanguageItemPreference(item = item) },
        )
    }
}

@Composable
private fun DownloadLanguagesHeaderPreference() {
    val learnMoreText =
        stringResource(id = R.string.download_languages_header_learn_more_preference)
    val learnMoreState = LinkTextState(
        text = learnMoreText,
        url = "www.mozilla.com",
        onClick = {},
    )
    Box(
        modifier = Modifier
            .padding(
                start = 72.dp,
                top = 6.dp,
                bottom = 6.dp,
                end = 16.dp,
            )
            .semantics { heading() },
    ) {
        LinkText(
            text = stringResource(
                R.string.download_languages_header_preference,
                learnMoreText,
            ),
            linkTextStates = listOf(learnMoreState),
            style = FirefoxTheme.typography.subtitle1.copy(
                color = FirefoxTheme.colors.textPrimary,
            ),
            linkTextDecoration = TextDecoration.Underline,
        )
    }
}

@Composable
private fun downloadLanguageItemContentDescriptionPreference(
    item: DownloadLanguageItemPreference,
    itemDescription: String,
): String {
    val contentDescription: String

    when (item.state.status) {
        DownloadLanguageItemStatusPreference.Downloaded -> {
            contentDescription =
                item.languageModel.language?.localizedDisplayName + " " + itemDescription + stringResource(
                    id = R.string.download_languages_item_content_description_downloaded_state,
                )
        }

        DownloadLanguageItemStatusPreference.NotDownloaded -> {
            contentDescription =
                item.languageModel.language?.localizedDisplayName + " " + itemDescription + " " + stringResource(
                    id = R.string.download_languages_item_content_description_not_downloaded_state,
                )
        }

        DownloadLanguageItemStatusPreference.DownloadInProgress -> {
            contentDescription =
                item.languageModel.language?.localizedDisplayName + " " + itemDescription + " " + stringResource(
                    id = R.string.download_languages_item_content_description_in_progress_state,
                )
        }

        DownloadLanguageItemStatusPreference.Selected -> {
            contentDescription =
                item.languageModel.language?.localizedDisplayName + " " + itemDescription + stringResource(
                    id = R.string.download_languages_item_content_description_selected_state,
                )
        }
    }
    return contentDescription
}

@Composable
private fun IconDownloadLanguageItemPreference(item: DownloadLanguageItemPreference) {
    when (item.state.status) {
        DownloadLanguageItemStatusPreference.Downloaded -> {
            if (item.state.type != DownloadLanguageItemTypePreference.PivotLanguage) {
                Icon(
                    painter = painterResource(
                        id = R.drawable.ic_delete,
                    ),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }

        DownloadLanguageItemStatusPreference.NotDownloaded -> {
            if (item.state.type != DownloadLanguageItemTypePreference.PivotLanguage) {
                Icon(
                    painter = painterResource(
                        id = R.drawable.ic_download,
                    ),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        }

        DownloadLanguageItemStatusPreference.DownloadInProgress -> {
            if (item.state.type != DownloadLanguageItemTypePreference.PivotLanguage) {
                DownloadIconIndicator(
                    icon = painterResource(id = R.drawable.mozac_ic_sync_24),
                )
            }
        }

        DownloadLanguageItemStatusPreference.Selected -> {}
    }
}

/**
 * List item used to display a label with description text (right next to the label) and
 * an [IconButton] at the end.
 *
 * @param label The label in the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param description An description text right next the label.
 * @param maxDescriptionLines An optional maximum number of lines for the description text to span.
 * @param enabled Controls the enabled state. When false, onClick,
 * and this modifier will appear disabled for accessibility services.
 * @param onClick Called when the user clicks on the item.
 * @param icon Composable for adding Icon UI.
 */
@Composable
private fun TextListItemInlineDescription(
    label: String,
    modifier: Modifier = Modifier,
    description: String,
    maxDescriptionLines: Int = 1,
    enabled: Boolean = true,
    onClick: (() -> Unit),
    icon: @Composable RowScope.() -> Unit = {},
) {
    Row(
        modifier = Modifier
            .defaultMinSize(minHeight = 56.dp)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = LocalIndication.current,
                enabled = enabled,
            ) { onClick.invoke() },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Row(
            modifier = modifier
                .padding(horizontal = 16.dp, vertical = 6.dp)
                .weight(1f),
        ) {
            Text(
                text = label,
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.subtitle1,
                maxLines = 1,
            )

            Text(
                text = description,
                modifier = Modifier.padding(horizontal = 5.dp),
                color = FirefoxTheme.colors.textSecondary,
                style = FirefoxTheme.typography.subtitle1,
                maxLines = maxDescriptionLines,
            )
        }
        IconButton(
            onClick = { onClick.invoke() },
            modifier = Modifier
                .padding(end = 16.dp)
                .size(24.dp)
                .clearAndSetSemantics {},
        ) {
            icon()
        }
    }
}

@Suppress("MagicNumber", "LongMethod")
@Composable
internal fun getLanguageListPreference(): List<DownloadLanguageItemPreference> {
    return mutableListOf<DownloadLanguageItemPreference>().apply {
        add(
            DownloadLanguageItemPreference(
                languageModel = TranslationsController.RuntimeTranslation.LanguageModel(
                    TranslationsController.Language(
                        Locale.CHINA.toLanguageTag(),
                        Locale.CHINA.displayLanguage,
                    ),
                    false,
                    4000,
                ),
                state = DownloadLanguageItemStatePreference(
                    type = DownloadLanguageItemTypePreference.GeneralLanguage,
                    status = DownloadLanguageItemStatusPreference.Downloaded,
                ),
            ),
        )
        add(
            DownloadLanguageItemPreference(
                languageModel = TranslationsController.RuntimeTranslation.LanguageModel(
                    TranslationsController.Language(
                        Locale.KOREAN.toLanguageTag(),
                        Locale.KOREAN.displayLanguage,
                    ),
                    false,
                    3000,
                ),
                state = DownloadLanguageItemStatePreference(
                    type = DownloadLanguageItemTypePreference.GeneralLanguage,
                    status = DownloadLanguageItemStatusPreference.NotDownloaded,
                ),
            ),
        )

        add(
            DownloadLanguageItemPreference(
                languageModel = TranslationsController.RuntimeTranslation.LanguageModel(
                    TranslationsController.Language(
                        Locale.FRANCE.toLanguageTag(),
                        Locale.FRANCE.displayLanguage,
                    ),
                    false,
                    2000,
                ),
                state = DownloadLanguageItemStatePreference(
                    type = DownloadLanguageItemTypePreference.GeneralLanguage,
                    status = DownloadLanguageItemStatusPreference.NotDownloaded,
                ),
            ),
        )
        add(
            DownloadLanguageItemPreference(
                languageModel = TranslationsController.RuntimeTranslation.LanguageModel(
                    TranslationsController.Language(
                        Locale.ITALIAN.toLanguageTag(),
                        Locale.ITALIAN.displayLanguage,
                    ),
                    false,
                    1000,
                ),
                state = DownloadLanguageItemStatePreference(
                    type = DownloadLanguageItemTypePreference.GeneralLanguage,
                    status = DownloadLanguageItemStatusPreference.DownloadInProgress,
                ),
            ),
        )
        add(
            DownloadLanguageItemPreference(
                languageModel = TranslationsController.RuntimeTranslation.LanguageModel(
                    TranslationsController.Language(
                        Locale.ENGLISH.toLanguageTag(),
                        Locale.ENGLISH.displayLanguage,
                    ),
                    true,
                    3000,
                ),
                state = DownloadLanguageItemStatePreference(
                    type = DownloadLanguageItemTypePreference.PivotLanguage,
                    status = DownloadLanguageItemStatusPreference.Selected,
                ),
            ),
        )
        add(
            DownloadLanguageItemPreference(
                languageModel = TranslationsController.RuntimeTranslation.LanguageModel(
                    TranslationsController.Language(
                        Locale.JAPANESE.toLanguageTag(),
                        Locale.JAPANESE.displayLanguage,
                    ),
                    true,
                    3000,
                ),
                state = DownloadLanguageItemStatePreference(
                    type = DownloadLanguageItemTypePreference.GeneralLanguage,
                    status = DownloadLanguageItemStatusPreference.NotDownloaded,
                ),
            ),
        )
        add(
            DownloadLanguageItemPreference(
                languageModel = TranslationsController.RuntimeTranslation.LanguageModel(
                    TranslationsController.Language(
                        stringResource(id = R.string.download_language_all_languages_item_preference),
                        stringResource(id = R.string.download_language_all_languages_item_preference),
                    ),
                    true,
                    300000,
                ),
                state = DownloadLanguageItemStatePreference(
                    type = DownloadLanguageItemTypePreference.AllLanguages,
                    status = DownloadLanguageItemStatusPreference.NotDownloaded,
                ),
            ),
        )
    }
}

@Composable
@LightDarkPreview
private fun DownloadLanguagesPreferencePreview() {
    FirefoxTheme {
        DownloadLanguagesPreference(
            downloadLanguageItemPreferences = getLanguageListPreference(),
            onItemClick = {},
        )
    }
}
