# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
from fluent.migrate.helpers import transforms_from
import fluent.syntax.ast as FTL
from fluent.migrate import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT
from fluent.migrate.helpers import COPY, VARIABLE_REFERENCE


def migrate(ctx):
    """Bug 1703012: fix control center and identity panel proton styling - part {index}"""
    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("identity-site-information"),
                value=REPLACE(
                    "browser/chrome/browser/browser.properties",
                    "identity.headerMainWithHost",
                    {
                        "%1$S": VARIABLE_REFERENCE("host"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("identity-header-security-with-host"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=REPLACE(
                            "browser/chrome/browser/browser.properties",
                            "identity.headerSecurityWithHost",
                            {
                                "%1$S": VARIABLE_REFERENCE("host"),
                            },
                        ),
                    ),
                ],
            ),
        ],
    )
