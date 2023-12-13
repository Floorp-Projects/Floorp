# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1869389 - Provide urlbar-search-mode-indicator-close button with an accessible name, part {index}"""

    file = "browser/browser/browser.ftl"
    ctx.add_transforms(
        file,
        file,
        transforms_from(
            """
urlbar-search-mode-indicator-close =
    .aria-label = {COPY_PATTERN(from_path, "ui-tour-info-panel-close.tooltiptext")}
""",
            from_path=file,
        ),
    )
