# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1534799 - Convert searchTextBox.clear.label to Fluent, part {index}"""
    target = "toolkit/toolkit/global/textActions.ftl"
    reference = "toolkit/toolkit/global/textActions.ftl"
    ctx.add_transforms(
        target,
        reference,
        transforms_from(
            """
text-action-search-text-box-clear =
    .title = { COPY(from_path, "searchTextBox.clear.label") }
""",
            from_path="toolkit/chrome/global/textcontext.dtd",
        ),
    )
