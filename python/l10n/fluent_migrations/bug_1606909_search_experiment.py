# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import REPLACE


def migrate(ctx):
    """Bug 1606909 - Port Tips from the experiment behind a pref, part {index}."""


    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("urlbar-search-tips-onboard"),
                value=REPLACE(
                    "browser/chrome/browser/browser.properties",
                    "urlbarSearchTip.onboarding",
                    {
                        "%1$S": VARIABLE_REFERENCE("engineName")
                    },
                    normalize_printf=True,
                    trim=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("urlbar-search-tips-redirect"),
                value=REPLACE(
                    "browser/chrome/browser/browser.properties",
                    "urlbarSearchTip.engineIsCurrentPage",
                    {
                        "%1$S": VARIABLE_REFERENCE("engineName")
                    },
                    normalize_printf=True,
                    trim=True
                )
            ),
        ]
    )
