# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY, REPLACE
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE


def migrate(ctx):
    """Bug 1763313 - Move storage.dtd to fluent, part {index}."""

    ctx.add_transforms(
        "devtools/client/storage.ftl",
        "devtools/client/storage.ftl",
        transforms_from(
            """
storage-search-box =
    .placeholder = { COPY(from_path, "searchBox.placeholder") }
storage-context-menu-delete-all =
  .label = { COPY(from_path, "storage.popupMenu.deleteAllLabel") }
storage-context-menu-delete-all-session-cookies =
  .label = { COPY(from_path, "storage.popupMenu.deleteAllSessionCookiesLabel") }
storage-context-menu-copy =
  .label = { COPY(from_path, "storage.popupMenu.copyLabel") }
""",
            from_path="devtools/client/storage.dtd",
        ),
    )

    ctx.add_transforms(
        "devtools/client/storage.ftl",
        "devtools/client/storage.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("storage-context-menu-delete"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "devtools/client/storage.properties",
                            "storage.popupMenu.deleteLabel",
                            {
                                "%1$S": VARIABLE_REFERENCE("itemName"),
                            },
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("storage-context-menu-delete-all-from"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            "devtools/client/storage.properties",
                            "storage.popupMenu.deleteAllFromLabel",
                            {
                                "%1$S": VARIABLE_REFERENCE("host"),
                            },
                        ),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("storage-idb-delete-blocked"),
                value=REPLACE(
                    "devtools/client/storage.properties",
                    "storage.idb.deleteBlocked",
                    {"%1$S": VARIABLE_REFERENCE("dbName")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("storage-idb-delete-error"),
                value=REPLACE(
                    "devtools/client/storage.properties",
                    "storage.idb.deleteError",
                    {"%1$S": VARIABLE_REFERENCE("dbName")},
                ),
            ),
        ],
    )

    ctx.add_transforms(
        "devtools/client/storage.ftl",
        "devtools/client/storage.ftl",
        transforms_from(
            """
storage-filter-key = { COPY(from_path, "storage.filter.key") }
storage-add-button =
  .title = { COPY(from_path, "storage.popupMenu.addItemLabel") }
storage-refresh-button =
  .title = { COPY(from_path, "storage.popupMenu.refreshItemLabel") }
storage-variable-view-search-box =
  .placeholder = { COPY(from_path, "storage.search.placeholder") }
storage-context-menu-add-item =
  .label = { COPY(from_path, "storage.popupMenu.addItemLabel") }
storage-expand-pane =
  .title = { COPY(from_path, "storage.expandPane") }
storage-collapse-pane =
  .title = { COPY(from_path, "storage.collapsePane") }
storage-expires-session = { COPY(from_path, "label.expires.session") }
storage-tree-labels-cookies = { COPY(from_path, "tree.labels.cookies") }
storage-tree-labels-local-storage = { COPY(from_path, "tree.labels.localStorage") }
storage-tree-labels-session-storage = { COPY(from_path, "tree.labels.sessionStorage") }
storage-tree-labels-indexed-db = { COPY(from_path, "tree.labels.indexedDB") }
storage-tree-labels-cache = { COPY(from_path, "tree.labels.Cache") }
storage-tree-labels-extension-storage = { COPY(from_path, "tree.labels.extensionStorage") }
storage-table-headers-cookies-name = { COPY(from_path, "table.headers.cookies.name") }
storage-table-headers-cookies-value = { COPY(from_path, "table.headers.cookies.value") }
storage-table-headers-cookies-expires = { COPY(from_path, "table.headers.cookies.expires2") }
storage-table-headers-cookies-size = { COPY(from_path, "table.headers.cookies.size") }
storage-table-headers-cookies-last-accessed = { COPY(from_path, "table.headers.cookies.lastAccessed2") }
storage-table-headers-cookies-creation-time = { COPY(from_path, "table.headers.cookies.creationTime2") }
storage-table-headers-cache-status = { COPY(from_path, "table.headers.Cache.status") }
storage-table-headers-extension-storage-area = { COPY(from_path, "table.headers.extensionStorage.area") }
storage-data = { COPY(from_path, "storage.data.label") }
storage-parsed-value = { COPY(from_path, "storage.parsedValue.label") }
""",
            from_path="devtools/client/storage.properties",
        ),
    )
