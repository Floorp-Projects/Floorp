# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1424683 - Migrate brand-short-name to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/branding/official/brand.ftl',
        'browser/branding/official/locales/en-US/brand.ftl',
        [
            FTL.Term(
                id=FTL.Identifier('-brand-short-name'),
                value=COPY(
                    'browser/branding/official/brand.dtd',
                    'brandShortName'
                )
            ),
        ]
    )
