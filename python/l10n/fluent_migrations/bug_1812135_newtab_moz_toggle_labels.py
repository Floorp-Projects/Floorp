# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1812135 - Convert some newtab customiation panel strings to use a label, part {index}"""

    translations_ftl = "browser/browser/newtab/newtab.ftl"

    ctx.add_transforms(
        translations_ftl,
        translations_ftl,
        transforms_from(
            """
newtab-custom-shortcuts-toggle =
  .label = {COPY_PATTERN(from_path, "newtab-custom-shortcuts-title")}
  .description = {COPY_PATTERN(from_path, "newtab-custom-shortcuts-subtitle")}
newtab-custom-pocket-toggle =
  .label = {COPY_PATTERN(from_path, "newtab-custom-pocket-title")}
  .description = {COPY_PATTERN(from_path, "newtab-custom-pocket-subtitle")}
newtab-custom-recent-toggle =
  .label = {COPY_PATTERN(from_path, "newtab-custom-recent-title")}
  .description = {COPY_PATTERN(from_path, "newtab-custom-recent-subtitle")}
""",
            from_path=translations_ftl,
        ),
    )
