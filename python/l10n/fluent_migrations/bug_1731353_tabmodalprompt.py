# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import COPY


def migrate(ctx):
    """Bug 1731353 - Migrate tabprompts strings from DTD to FTL, part {index}"""

    ctx.add_transforms(
        "toolkit/toolkit/global/tabprompts.ftl",
        "toolkit/toolkit/global/tabprompts.ftl",
        transforms_from(
            """

tabmodalprompt-username =
    .value = { COPY(path1, "editfield0.label") }
tabmodalprompt-password =
    .value = { COPY(path1, "editfield1.label") }
""",
            path1="toolkit/chrome/global/commonDialog.dtd",
        ),
    )

    ctx.add_transforms(
        "toolkit/toolkit/global/tabprompts.ftl",
        "toolkit/toolkit/global/tabprompts.ftl",
        transforms_from(
            """

tabmodalprompt-ok-button =
    .label = { COPY(path1, "okButton.label") }
tabmodalprompt-cancel-button =
    .label = { COPY(path1, "cancelButton.label") }
""",
            path1="toolkit/chrome/global/dialogOverlay.dtd",
        ),
    )
