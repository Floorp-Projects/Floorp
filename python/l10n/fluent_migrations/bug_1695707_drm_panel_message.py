# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, CONCAT, REPLACE
from fluent.migrate.helpers import TERM_REFERENCE


def migrate(ctx):
    """Bug 1695707 - Migrate the DRM panel message to Fluent, part {index}"""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("eme-notifications-drm-content-playing"),
                value=REPLACE(
                    "browser/chrome/browser/browser.properties",
                    "emeNotifications.drmContentPlaying.message2",
                    {
                        "%1$S": TERM_REFERENCE("brand-short-name"),
                    },
                    normalize_printf=True,
                ),
            ),
        ],
    )
