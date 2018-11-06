# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1502396 - Convert change and remove master password dialogs in about:preferences to use Fluent"""

    ctx.add_transforms(
        "toolkit/toolkit/preferences/preferences.ftl",
        "toolkit/toolkit/preferences/preferences.ftl",
        transforms_from(
            """
set-password =
    .title = { COPY(from_path, "setPassword.title") }
set-password-old-password = { COPY(from_path, "setPassword.oldPassword.label") }
set-password-new-password = { COPY(from_path, "setPassword.newPassword.label") }
set-password-reenter-password = { COPY(from_path, "setPassword.reenterPassword.label") }
set-password-meter = { COPY(from_path, "setPassword.meter.label") }
set-password-meter-loading = { COPY(from_path, "setPassword.meter.loading") }
master-password-description = { COPY(from_path, "masterPasswordDescription.label") }
master-password-warning = { COPY(from_path, "masterPasswordWarning.label") }
""", from_path="toolkit/chrome/mozapps/preferences/changemp.dtd"))

    ctx.add_transforms(
        "toolkit/toolkit/preferences/preferences.ftl",
        "toolkit/toolkit/preferences/preferences.ftl",
        transforms_from(
            """
remove-password =
    .title = { COPY(from_path, "removePassword.title") }
remove-info =
    .label = { COPY(from_path, "removeInfo.label") }
remove-warning1 = { COPY(from_path, "removeWarning1.label") }
remove-warning2 = { COPY(from_path, "removeWarning2.label") }
remove-password-old-password =
    .label = { COPY(from_path, "setPassword.oldPassword.label") }
""", from_path="toolkit/chrome/mozapps/preferences/removemp.dtd"))
