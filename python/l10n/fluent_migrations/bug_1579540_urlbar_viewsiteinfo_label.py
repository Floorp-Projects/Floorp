# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from

def migrate(ctx):
    """Bug 1579540 - Migrate urlbar.viewSiteInfo.label from dtd to ftl, part {index}"""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
"""
browser-urlbar-identity-button =
    .aria-label = { COPY(from_path, "urlbar.viewSiteInfo.label") }
""", from_path="browser/chrome/browser/browser.dtd"))
