# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from


def migrate(ctx):
    """Bug 1476034 - about:crashes localization migration from DTD to Fluent"""

    ctx.add_transforms(
        "toolkit/crashreporter/aboutcrashes.ftl",
        "toolkit/crashreporter/aboutcrashes.ftl",
        transforms_from(
"""
crash-reports-title = { COPY("toolkit/crashreporter/crashes.dtd", "crashReports.title") }

clear-all-reports-label = { COPY("toolkit/crashreporter/crashes.dtd", "clearAllReports.label") }
delete-confirm-title = { COPY("toolkit/crashreporter/crashes.properties", "deleteconfirm.title") }
delete-confirm-description = { COPY("toolkit/crashreporter/crashes.properties", "deleteconfirm.description") }

crashes-unsubmitted-label = { COPY("toolkit/crashreporter/crashes.dtd", "crashesUnsubmitted.label") }
id-heading = { COPY("toolkit/crashreporter/crashes.dtd", "id.heading") }
date-crashed-heading = { COPY("toolkit/crashreporter/crashes.dtd", "dateCrashed.heading") }

crashes-submitted-label = { COPY("toolkit/crashreporter/crashes.dtd", "crashesSubmitted.label") }
date-submitted-heading = { COPY("toolkit/crashreporter/crashes.dtd", "dateSubmitted.heading") }

no-reports-label = { COPY("toolkit/crashreporter/crashes.dtd", "noReports.label") }
no-config-label = { COPY("toolkit/crashreporter/crashes.dtd", "noConfig.label") }
""")
    )
