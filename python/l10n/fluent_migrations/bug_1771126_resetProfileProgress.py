# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1771126 - Migrate resetProfileProgress strings from DTD to FTL, part {index}"""

    source = "toolkit/chrome/global/resetProfile.dtd"
    target = "toolkit/toolkit/global/resetProfile.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("refresh-profile-progress"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=REPLACE(
                            source,
                            "refreshProfile.dialog.title",
                            {
                                "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                            },
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("refresh-profile-progress-description"),
                value=COPY(source, "refreshProfile.cleaning.description"),
            ),
        ],
    )
