# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, REPLACE
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE

def migrate(ctx):
    """Bug 1608200 - Migrate wizard to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/global/wizard.ftl",
        "toolkit/toolkit/global/wizard.ftl",
    transforms_from(
"""
wizard-macos-button-back =
    .label = { COPY(from_path, "button-back-mac.label") }
    .accesskey = { COPY(from_path, "button-back-mac.accesskey") }
wizard-linux-button-back =
    .label = { COPY(from_path, "button-back-unix.label") }
    .accesskey = { COPY(from_path, "button-back-unix.accesskey") }
wizard-win-button-back =
    .label = { COPY(from_path, "button-back-win.label") }
    .accesskey = { COPY(from_path, "button-back-win.accesskey") }
wizard-macos-button-next =
    .label = { COPY(from_path, "button-next-mac.label") }
    .accesskey = { COPY(from_path, "button-next-mac.accesskey") }
wizard-linux-button-next =
    .label = { COPY(from_path, "button-next-unix.label") }
    .accesskey = { COPY(from_path, "button-next-unix.accesskey") }
wizard-win-button-next =
    .label = { COPY(from_path, "button-next-win.label") }
    .accesskey = { COPY(from_path, "button-next-win.accesskey") }
wizard-macos-button-finish =
    .label = { COPY(from_path, "button-finish-mac.label") }
wizard-linux-button-finish =
    .label = { COPY(from_path, "button-finish-unix.label") }
wizard-win-button-finish =
    .label = { COPY(from_path, "button-finish-win.label") }
wizard-macos-button-cancel =
    .label = { COPY(from_path, "button-cancel-mac.label") }
wizard-linux-button-cancel =
    .label = { COPY(from_path, "button-cancel-unix.label") }
wizard-win-button-cancel =
    .label = { COPY(from_path, "button-cancel-win.label") }
""", from_path="toolkit/chrome/global/wizard.dtd"))
