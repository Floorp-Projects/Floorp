# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, CONCAT, REPLACE

def migrate(ctx):
    """Bug 1608202 - Migrate downloads to Fluent, part {index}."""

    ctx.add_transforms(
        "browser/browser/downloads.ftl",
        "browser/browser/downloads.ftl",
    transforms_from(
"""
downloads-window =
    .title = { COPY(from_path, "downloads.title") }
downloads-panel =
    .aria-label = { COPY(from_path, "downloads.title") }
downloads-panel-list =
    .style = width: { COPY(from_path, "downloads.width") }
downloads-cmd-pause =
    .label = { COPY(from_path, "cmd.pause.label") }
    .accesskey = { COPY(from_path, "cmd.pause.accesskey") }
downloads-cmd-resume =
    .label = { COPY(from_path, "cmd.resume.label") }
    .accesskey = { COPY(from_path, "cmd.resume.accesskey") }
downloads-cmd-cancel =
    .tooltiptext = { COPY(from_path, "cmd.cancel.label") }
downloads-cmd-cancel-panel =
    .aria-label = { COPY(from_path, "cmd.cancel.label") }
downloads-cmd-show =
    .label = { COPY(from_path, "cmd.show.label") }
    .tooltiptext = { downloads-cmd-show.label }
    .accesskey = { COPY(from_path, "cmd.show.accesskey") }
downloads-cmd-show-mac =
    .label = { COPY(from_path, "cmd.showMac.label") }
    .tooltiptext = { downloads-cmd-show-mac.label }
    .accesskey = { COPY(from_path, "cmd.showMac.accesskey") }
downloads-cmd-show-panel =
    .aria-label = { PLATFORM() ->
        [macos] { COPY(from_path, "cmd.showMac.label") }
       *[other] { COPY(from_path, "cmd.show.label") }
    }
downloads-cmd-show-description =
    .value = { PLATFORM() ->
        [macos] { COPY(from_path, "cmd.showMac.label") }
       *[other] { COPY(from_path, "cmd.show.label") }
    }
downloads-cmd-show-downloads =
    .label = { COPY(from_path, "cmd.showDownloads.label") }
downloads-cmd-retry =
    .tooltiptext = { COPY(from_path, "cmd.retry.label") }
downloads-cmd-retry-panel =
    .aria-label = { COPY(from_path, "cmd.retry.label") }
downloads-cmd-go-to-download-page =
    .label = { COPY(from_path, "cmd.goToDownloadPage.label") }
    .accesskey = { COPY(from_path, "cmd.goToDownloadPage.accesskey") }
downloads-cmd-copy-download-link =
    .label = { COPY(from_path, "cmd.copyDownloadLink.label") }
    .accesskey = { COPY(from_path, "cmd.copyDownloadLink.accesskey") }
downloads-cmd-remove-from-history =
    .label = { COPY(from_path, "cmd.removeFromHistory.label") }
    .accesskey = { COPY(from_path, "cmd.removeFromHistory.accesskey") }
downloads-cmd-clear-list =
    .label = { COPY(from_path, "cmd.clearList2.label") }
    .accesskey = { COPY(from_path, "cmd.clearList2.accesskey") }
downloads-cmd-clear-downloads =
    .label = { COPY(from_path, "cmd.clearDownloads.label") }
    .accesskey = { COPY(from_path, "cmd.clearDownloads.accesskey") }
downloads-cmd-unblock =
    .label = { COPY(from_path, "cmd.unblock2.label") }
    .accesskey = { COPY(from_path, "cmd.unblock2.accesskey") }
downloads-cmd-remove-file =
    .tooltiptext = { COPY(from_path, "cmd.removeFile.label") }
downloads-cmd-remove-file-panel =
    .aria-label = { COPY(from_path, "cmd.removeFile.label") }
downloads-cmd-choose-unblock =
    .tooltiptext = { COPY(from_path, "cmd.chooseUnblock.label") }
downloads-cmd-choose-unblock-panel =
    .aria-label = { COPY(from_path, "cmd.chooseUnblock.label") }
downloads-cmd-choose-open =
    .tooltiptext = { COPY(from_path, "cmd.chooseOpen.label") }
downloads-cmd-choose-open-panel =
    .aria-label = { COPY(from_path, "cmd.chooseOpen.label") }
downloads-show-more-information =
    .value = { COPY(from_path, "showMoreInformation.label") }
downloads-open-file =
    .value = { COPY(from_path, "openFile.label") }
downloads-retry-download =
    .value = { COPY(from_path, "retryDownload.label") }
downloads-cancel-download =
    .value = { COPY(from_path, "cancelDownload.label") }
downloads-history =
    .label = { COPY(from_path, "downloadsHistory.label") }
    .accesskey = { COPY(from_path, "downloadsHistory.accesskey") }
downloads-details =
    .title = { COPY(from_path, "downloadDetails.label") }
downloads-clear-downloads-button =
    .label = { COPY(from_path, "clearDownloadsButton.label") }
    .tooltiptext = { COPY(from_path, "clearDownloadsButton.tooltip") }
downloads-list-empty =
    .value = { COPY(from_path, "downloadsListEmpty.label") }
downloads-panel-empty =
    .value = { COPY(from_path, "downloadsPanelEmpty.label") }
""", from_path="browser/chrome/browser/downloads/downloads.dtd"))
