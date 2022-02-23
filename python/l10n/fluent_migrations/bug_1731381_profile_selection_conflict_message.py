# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate import REPLACE


def migrate(ctx):
    """Bug 1731381  - Migrate profileSelection conflict message from properties to Fluent, part {index}."""
    ctx.add_transforms(
        "toolkit/toolkit/global/profileSelection.ftl",
        "toolkit/toolkit/global/profileSelection.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("profile-selection-conflict-message"),
                value=REPLACE(
                    "toolkit/chrome/mozapps/profile/profileSelection.properties",
                    "conflictMessage",
                    {
                        "%1$S": TERM_REFERENCE("brand-product-name"),
                        "%2$S": TERM_REFERENCE("brand-short-name"),
                    },
                ),
            ),
        ],
    )
