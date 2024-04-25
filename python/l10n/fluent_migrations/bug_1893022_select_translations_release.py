# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1893022 - Move Select Translations Fluent Strings out of Preview, part {index}"""

    translations_ftl = "browser/browser/translations.ftl"

    ctx.add_transforms(
        translations_ftl,
        translations_ftl,
        transforms_from(
            """
select-translations-panel-translation-failure-message =
    .message = {COPY_PATTERN(from_path, "translations-panel-error-translating")}
select-translations-panel-unsupported-language-message-known =
    .message = {COPY_PATTERN(from_path, "translations-panel-error-unsupported-hint-known")}
select-translations-panel-unsupported-language-message-unknown =
    .message = {COPY_PATTERN(from_path, "translations-panel-error-unsupported-hint-unknown")}

select-translations-panel-cancel-button =
    .label = {COPY_PATTERN(from_path, "translations-panel-translate-cancel.label")}
select-translations-panel-translate-button =
    .label = {COPY_PATTERN(from_path, "translations-panel-translate-button.label")}
select-translations-panel-try-again-button =
    .label = {COPY_PATTERN(from_path, "translations-panel-error-load-languages-hint-button.label")}
""",
            from_path=translations_ftl,
        ),
    )
