# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY


def migrate(ctx):
    """Bug 1833228 - Remove "moz-" from moz-tab-list component and associated .ftl, .css files, part {index}."""
    ctx.add_transforms(
        "browser/browser/fxviewTabList.ftl",
        "browser/browser/fxviewTabList.ftl",
        transforms_from(
            """
fxviewtabrow-open-menu-button =
   .title = {COPY_PATTERN(from_path, "mztabrow-open-menu-button.title")}
fxviewtabrow-date = {COPY_PATTERN(from_path, "mztabrow-date")}
fxviewtabrow-time = {COPY_PATTERN(from_path, "mztabrow-time")}
fxviewtabrow-tabs-list-tab =
   .title = {COPY_PATTERN(from_path, "mztabrow-tabs-list-tab.title")}
fxviewtabrow-dismiss-tab-button =
   .title = {COPY_PATTERN(from_path, "mztabrow-dismiss-tab-button.title")}
fxviewtabrow-just-now-timestamp = {COPY_PATTERN(from_path, "mztabrow-just-now-timestamp")}
fxviewtabrow-delete = {COPY_PATTERN(from_path, "mztabrow-delete")}
    .accesskey = {COPY_PATTERN(from_path, "mztabrow-delete.accesskey")}
fxviewtabrow-forget-about-this-site = {COPY_PATTERN(from_path, "mztabrow-forget-about-this-site")}
    .accesskey = {COPY_PATTERN(from_path, "mztabrow-forget-about-this-site.accesskey")}
fxviewtabrow-open-in-window = {COPY_PATTERN(from_path, "mztabrow-open-in-window")}
    .accesskey = {COPY_PATTERN(from_path, "mztabrow-open-in-window.accesskey")}
fxviewtabrow-open-in-private-window = {COPY_PATTERN(from_path, "mztabrow-open-in-private-window")}
    .accesskey = {COPY_PATTERN(from_path, "mztabrow-open-in-private-window.accesskey")}
fxviewtabrow-add-bookmark = {COPY_PATTERN(from_path, "mztabrow-add-bookmark")}
    .accesskey = {COPY_PATTERN(from_path, "mztabrow-add-bookmark.accesskey")}
fxviewtabrow-save-to-pocket = {COPY_PATTERN(from_path, "mztabrow-save-to-pocket")}
    .accesskey = {COPY_PATTERN(from_path, "mztabrow-save-to-pocket.accesskey")}
fxviewtabrow-copy-link = {COPY_PATTERN(from_path, "mztabrow-copy-link")}
    .accesskey = {COPY_PATTERN(from_path, "mztabrow-copy-link.accesskey")}
    """,
            from_path="browser/browser/mozTabList.ftl",
        ),
    )
