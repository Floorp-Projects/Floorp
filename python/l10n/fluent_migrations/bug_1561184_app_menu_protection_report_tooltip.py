# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1561184 - Show tooltip text for protection report in app menu, part {index}"""

    ctx.add_transforms(
        'browser/browser/appmenu.ftl',
        'browser/browser/appmenu.ftl',
        transforms_from(
"""
appmenuitem-protection-report-title = { COPY(from_path, "protectionReport.title") }
appmenuitem-protection-report-tooltip =
    .tooltiptext = { COPY(from_path, "protectionReport.tooltip") }
""", from_path="browser/chrome/browser/browser.properties"))
