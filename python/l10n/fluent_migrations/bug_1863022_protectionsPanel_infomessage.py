# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1863022 - Move Protection Panel Message to calling code, part {index}"""

    source = "browser/browser/newtab/asrouter.ftl"
    target = "browser/browser/protectionsPanel.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
cfr-protections-panel-header = {COPY_PATTERN(from_path, "cfr-protections-panel-header")}
cfr-protections-panel-body = {COPY_PATTERN(from_path, "cfr-protections-panel-body")}
cfr-protections-panel-link-text = {COPY_PATTERN(from_path, "cfr-protections-panel-link-text")}
""",
            from_path=source,
        ),
    )
