# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1432338 - Introduce sync-brand.ftl, part {index}."""

    ctx.add_transforms(
        'browser/browser/branding/sync-brand.ftl',
        'browser/browser/branding/sync-brand.ftl',
        [
            FTL.Term(
                id=FTL.Identifier('-sync-brand-short-name'),
                value=COPY(
                    'browser/chrome/browser/syncBrand.dtd',
                    'syncBrand.shortName.label'
                )
            ),
            FTL.Term(
                id=FTL.Identifier('-sync-brand-name'),
                value=COPY(
                    'browser/chrome/browser/syncBrand.dtd',
                    'syncBrand.fullName.label'
                )
            ),
        ]
    )
