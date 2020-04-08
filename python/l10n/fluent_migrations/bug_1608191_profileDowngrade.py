# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import REPLACE
from fluent.migrate.helpers import COPY, TERM_REFERENCE, transforms_from

def migrate(ctx):
    """Bug 1608191 - Port profileDowngrade.dtd to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/global/profileDowngrade.ftl",
        "toolkit/toolkit/global/profileDowngrade.ftl",
    transforms_from(
"""
profiledowngrade-window-create =
    .label = { COPY(from_path, "window.create") }
profiledowngrade-quit =
    .label = { PLATFORM() ->
                 [windows] { COPY(from_path, "window.quit-win") }
                *[other] { COPY(from_path, "window.quit-nonwin") }
             }
""", from_path="toolkit/chrome/mozapps/profile/profileDowngrade.dtd"))

    ctx.add_transforms(
        "toolkit/toolkit/global/profileDowngrade.ftl",
        "toolkit/toolkit/global/profileDowngrade.ftl",
        [
                FTL.Message(
                    id=FTL.Identifier("profiledowngrade-window"),
                    attributes=[
                        FTL.Attribute(
                            id=FTL.Identifier("title"),
                            value=REPLACE(
                                "toolkit/chrome/mozapps/profile/profileDowngrade.dtd",
                                "window.title",
                                {
                                    "Firefox": TERM_REFERENCE("brand-product-name"),
                                }
                            )
                        ),
                        FTL.Attribute(
                            id=FTL.Identifier("style"),
                            value=COPY("toolkit/chrome/mozapps/profile/profileDowngrade.dtd", "window.style")
                            )
                    ]
                ),
                FTL.Message(
                    id=FTL.Identifier("profiledowngrade-sync"),
                    value=REPLACE(
                        "toolkit/chrome/mozapps/profile/profileDowngrade.dtd",
                        "window.sync",
                        {
                            "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                            "Firefox": TERM_REFERENCE("brand-product-name"),
                            "&syncBrand.fxAccount.label;": TERM_REFERENCE("fxaccount-brand-name"),
                        },
                    ),
                ),
            FTL.Message(
                id=FTL.Identifier("profiledowngrade-nosync"),
                value=REPLACE(
                    "toolkit/chrome/mozapps/profile/profileDowngrade.dtd",
                    "window.nosync",
                    {
                        "&brandShortName;": TERM_REFERENCE("brand-short-name"),
                        "Firefox": TERM_REFERENCE("brand-product-name"),
                    }
                ),
            ),
        ]
    )

