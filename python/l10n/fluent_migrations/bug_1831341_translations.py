# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1831341 - Convert translations-panel-settings-about to use a label, part {index}"""

    translations_ftl = "browser/browser/translations.ftl"

    ctx.add_transforms(
        translations_ftl,
        translations_ftl,
        transforms_from(
            """
translations-panel-settings-about2 =
  .label = {COPY_PATTERN(from_path, "translations-panel-settings-about")}
""",
            from_path=translations_ftl,
        ),
    )
