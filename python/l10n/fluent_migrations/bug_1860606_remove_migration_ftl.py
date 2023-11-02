# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1860606 - Get rid of migration.ftl in favour of migrationWizard.ftl, part {index}."""

    source = "browser/browser/migration.ftl"
    target = "browser/browser/migrationWizard.ftl"

    ctx.add_transforms(
        target,
        target,
        transforms_from(
            """
migration-source-name-ie = {COPY_PATTERN(from_path, "source-name-ie")}
migration-source-name-edge = {COPY_PATTERN(from_path, "source-name-edge")}
migration-source-name-chrome = {COPY_PATTERN(from_path, "source-name-chrome")}

migration-imported-safari-reading-list = {COPY_PATTERN(from_path, "imported-safari-reading-list")}
migration-imported-edge-reading-list = {COPY_PATTERN(from_path, "imported-edge-reading-list")}
""",
            from_path=source,
        ),
    )
