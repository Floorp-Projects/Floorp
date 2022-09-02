# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1734220 - Migrate translation-notification.js to Fluent, part {index}"""

    target = "browser/browser/translationNotification.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """

translation-notification-this-page-is-in =
    .value = { COPY(path1, "translation.thisPageIsIn.label") }
translation-notification-translate-this-page =
    .value = { COPY(path1, "translation.translateThisPage.label") }
translation-notification-translate-button =
    .label = { COPY(path1, "translation.translate.button") }
translation-notification-not-now-button =
    .label = { COPY(path1, "translation.notNow.button") }
translation-notification-translating-content =
    .value = { COPY(path1, "translation.translatingContent.label") }
translation-notification-translated-from =
    .value = { COPY(path1, "translation.translatedFrom.label") }
translation-notification-translated-to =
    .value = { COPY(path1, "translation.translatedTo.label") }
translation-notification-translated-to-suffix =
    .value = { COPY(path1, "translation.translatedToSuffix.label") }
translation-notification-show-original-button =
    .label = { COPY(path1, "translation.showOriginal.button") }
translation-notification-show-translation-button =
    .label = { COPY(path1, "translation.showTranslation.button") }
translation-notification-error-translating =
    .value = { COPY(path1, "translation.errorTranslating.label") }
translation-notification-try-again-button =
    .label = { COPY(path1, "translation.tryAgain.button") }
translation-notification-service-unavailable =
    .value = { COPY(path1, "translation.serviceUnavailable.label") }
translation-notification-options-menu =
    .label = { COPY(path1, "translation.options.menu") }
translation-notification-options-never-for-site =
    .label = { COPY(path1, "translation.options.neverForSite.label") }
    .accesskey = { COPY(path1, "translation.options.neverForSite.accesskey") }
translation-notification-options-preferences =
    .label = { COPY(path1, "translation.options.preferences.label") }
    .accesskey = { COPY(path1, "translation.options.preferences.accesskey") }
""",
            path1="browser/chrome/browser/translation.dtd",
        ),
    )

    source = "browser/chrome/browser/translation.properties"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier(
                    "translation-notification-options-never-for-language"
                ),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            source,
                            "translation.options.neverForLanguage.label",
                            {"%1$S": VARIABLE_REFERENCE("langName")},
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(
                            source, "translation.options.neverForLanguage.accesskey"
                        ),
                    ),
                ],
            ),
        ],
    )
