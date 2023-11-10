# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1862982 - Fix fluent schema for cookie banner cancel button, part {index}."""

    file = "browser/browser/protectionsPanel.ftl"
    ctx.add_transforms(
        file,
        file,
        transforms_from(
            """
protections-panel-cookie-banner-view-cancel-label =
  .label = {COPY_PATTERN(from_path, "protections-panel-cookie-banner-view-cancel")}
""",
            from_path=file,
        ),
    )
