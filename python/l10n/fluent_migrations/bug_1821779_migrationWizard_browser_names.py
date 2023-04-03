# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1821779 - Move existing browser names to migrationWizard.ftl, part {index}."""

    ctx.add_transforms(
        "browser/browser/migrationWizard.ftl",
        "browser/browser/migrationWizard.ftl",
        transforms_from(
            """
migration-wizard-migrator-display-name-brave = {COPY_PATTERN(from_path, "import-from-brave.label")}
migration-wizard-migrator-display-name-canary = {COPY_PATTERN(from_path, "import-from-canary.label")}
migration-wizard-migrator-display-name-chrome = {COPY_PATTERN(from_path, "import-from-chrome.label")}
migration-wizard-migrator-display-name-chrome-beta = {COPY_PATTERN(from_path, "import-from-chrome-beta.label")}
migration-wizard-migrator-display-name-chrome-dev = {COPY_PATTERN(from_path, "import-from-chrome-dev.label")}
migration-wizard-migrator-display-name-chromium = {COPY_PATTERN(from_path, "import-from-chromium.label")}
migration-wizard-migrator-display-name-chromium-360se = {COPY_PATTERN(from_path, "import-from-360se.label")}
migration-wizard-migrator-display-name-chromium-edge = {COPY_PATTERN(from_path, "import-from-edge.label")}
migration-wizard-migrator-display-name-chromium-edge-beta = {COPY_PATTERN(from_path, "import-from-edge-beta.label")}
migration-wizard-migrator-display-name-edge-legacy = {COPY_PATTERN(from_path, "import-from-edge-legacy.label")}
migration-wizard-migrator-display-name-firefox = {COPY_PATTERN(from_path, "import-from-firefox.label")}
migration-wizard-migrator-display-name-ie = {COPY_PATTERN(from_path, "import-from-ie.label")}
migration-wizard-migrator-display-name-opera = {COPY_PATTERN(from_path, "import-from-opera.label")}
migration-wizard-migrator-display-name-opera-gx = {COPY_PATTERN(from_path, "import-from-opera-gx.label")}
migration-wizard-migrator-display-name-safari = {COPY_PATTERN(from_path, "import-from-safari.label")}
migration-wizard-migrator-display-name-vivaldi = {COPY_PATTERN(from_path, "import-from-vivaldi.label")}
""",
            from_path="browser/browser/migration.ftl",
        ),
    )
