# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, TERM_REFERENCE, MESSAGE_REFERENCE
from fluent.migrate import COPY_PATTERN, REPLACE, COPY

def migrate(ctx):
    """Bug 1609562 - Migrate popup-notifications.inc to Fluent, part {index}."""

    ctx.add_transforms(
        'browser/browser/browser.ftl',
        'browser/browser/browser.ftl',
        transforms_from(
"""
popup-select-camera =
    .value = { COPY(from_path, "getUserMedia.selectCamera.label") }
    .accesskey = { COPY(from_path, "getUserMedia.selectCamera.accesskey") }
popup-select-microphone =
    .value = { COPY(from_path, "getUserMedia.selectMicrophone.label") }
    .accesskey = { COPY(from_path, "getUserMedia.selectMicrophone.accesskey") }
popup-all-windows-shared = { COPY(from_path, "getUserMedia.allWindowsShared.message") }
""", from_path="browser/chrome/browser/browser.dtd")
    )
