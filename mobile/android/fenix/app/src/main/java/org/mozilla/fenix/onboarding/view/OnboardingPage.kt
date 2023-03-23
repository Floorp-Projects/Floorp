/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.ClickableText
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.SecondaryButton
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The ratio of the image height to the window height. This was determined from the designs in figma
 * taking the ratio of the image height to the mockup height.
 */
private const val IMAGE_HEIGHT_RATIO = 0.4f

/**
 * The tag used for links in the text for annotated strings.
 */
private const val URL_TAG = "URL_TAG"

/**
 * A composable for displaying onboarding page content.
 *
 * @param pageState [OnboardingPageState] The page content that's displayed.
 * @param modifier The modifier to be applied to the Composable.
 * @param onDismiss Invoked when the user clicks the close button. This defaults to null. When null,
 * it doesn't show the close button.
 */
@Composable
fun OnboardingPage(
    pageState: OnboardingPageState,
    modifier: Modifier = Modifier,
    onDismiss: (() -> Unit)? = null,
) {
    BoxWithConstraints(
        modifier = Modifier
            .background(FirefoxTheme.colors.layer1)
            .padding(bottom = if (pageState.secondaryButton == null) 32.dp else 24.dp)
            .then(modifier),
    ) {
        val boxWithConstraintsScope = this
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState()),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.SpaceBetween,
        ) {
            if (onDismiss != null) {
                IconButton(
                    onClick = onDismiss,
                    modifier = Modifier.align(Alignment.End),
                ) {
                    Icon(
                        painter = painterResource(id = R.drawable.mozac_ic_close),
                        contentDescription = stringResource(R.string.onboarding_home_content_description_close_button),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            } else {
                Spacer(Modifier)
            }

            Column(
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 32.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Image(
                    painter = painterResource(id = pageState.image),
                    contentDescription = null,
                    modifier = Modifier
                        .height(boxWithConstraintsScope.maxHeight.times(IMAGE_HEIGHT_RATIO)),
                )

                Spacer(modifier = Modifier.height(32.dp))

                Text(
                    text = pageState.title,
                    color = FirefoxTheme.colors.textPrimary,
                    textAlign = TextAlign.Center,
                    style = FirefoxTheme.typography.headline5,
                )

                Spacer(modifier = Modifier.height(16.dp))

                DescriptionText(
                    description = pageState.description,
                    linkTextState = pageState.linkTextState,
                )
            }

            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.padding(horizontal = 16.dp),
            ) {
                PrimaryButton(
                    text = pageState.primaryButton.text,
                    onClick = pageState.primaryButton.onClick,
                )

                if (pageState.secondaryButton != null) {
                    Spacer(modifier = Modifier.height(8.dp))
                    SecondaryButton(
                        text = pageState.secondaryButton.text,
                        onClick = pageState.secondaryButton.onClick,
                    )
                }
            }

            LaunchedEffect(pageState) {
                pageState.onRecordImpressionEvent()
            }
        }
    }
}

@Composable
private fun DescriptionText(
    description: String,
    linkTextState: LinkTextState?,
) {
    if (linkTextState != null) {
        LinkText(
            text = description,
            linkTextState = linkTextState,
        )
    } else {
        Text(
            text = description,
            color = FirefoxTheme.colors.textSecondary,
            textAlign = TextAlign.Center,
            style = FirefoxTheme.typography.body2,
        )
    }
}

/**
 * A composable for displaying text that contains a clickable link text.
 *
 * @param text The complete text.
 * @param linkTextState The clickable part of the text.
 */
@Composable
private fun LinkText(
    text: String,
    linkTextState: LinkTextState,
) {
    val annotatedString = buildAnnotatedString {
        val startIndex = text.indexOf(linkTextState.text)
        val endIndex = startIndex + linkTextState.text.length
        append(text)
        addStyle(
            style = SpanStyle(color = FirefoxTheme.colors.textAccent),
            start = startIndex,
            end = endIndex,
        )

        addStringAnnotation(
            tag = URL_TAG,
            annotation = linkTextState.url,
            start = startIndex,
            end = endIndex,
        )
    }

    ClickableText(
        text = annotatedString,
        style = FirefoxTheme.typography.body2.copy(
            textAlign = TextAlign.Center,
            color = FirefoxTheme.colors.textSecondary,
        ),
        onClick = {
            val range: AnnotatedString.Range<String>? =
                annotatedString.getStringAnnotations(URL_TAG, it, it).firstOrNull()
            range?.let { stringAnnotation ->
                linkTextState.onClick(stringAnnotation.item)
            }
        },
    )
}

@LightDarkPreview
@Composable
private fun OnboardingPagePreview() {
    FirefoxTheme {
        OnboardingPage(
            pageState = OnboardingPageState(
                image = R.drawable.ic_notification_permission,
                title = stringResource(
                    id = R.string.onboarding_home_enable_notifications_title,
                    formatArgs = arrayOf(stringResource(R.string.app_name)),
                ),
                description = stringResource(
                    id = R.string.onboarding_home_enable_notifications_description,
                    formatArgs = arrayOf(stringResource(R.string.app_name)),
                ),
                primaryButton = Action(
                    text = stringResource(
                        id = R.string.onboarding_home_enable_notifications_positive_button,
                    ),
                    onClick = {},
                ),
                secondaryButton = Action(
                    text = stringResource(id = R.string.onboarding_home_enable_notifications_negative_button),
                    onClick = {},
                ),
                onRecordImpressionEvent = {},
            ),
            onDismiss = {},
        )
    }
}
