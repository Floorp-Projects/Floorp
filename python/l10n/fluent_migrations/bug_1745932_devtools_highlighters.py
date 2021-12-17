# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY, REPLACE
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE


def migrate(ctx):
    """Bug 1745932 - Move highlighters.properties to fluent, part {index}."""

    ctx.add_transforms(
        "devtools/shared/highlighters.ftl",
        "devtools/shared/highlighters.ftl",
        transforms_from(
            """
gridtype-container = { COPY(from_path, "gridType.container") }
gridtype-item = { COPY(from_path, "gridType.item") }
gridtype-dual = { COPY(from_path, "gridType.dual") }
flextype-container = { COPY(from_path, "flexType.container") }
flextype-item = { COPY(from_path, "flexType.item") }
flextype-dual = { COPY(from_path, "flexType.dual") }
remote-node-picker-notice-action-desktop = { COPY(from_path, "remote-node-picker-notice-action-desktop") }
remote-node-picker-notice-action-touch = { COPY(from_path, "remote-node-picker-notice-action-touch") }
remote-node-picker-notice-hide-button = { COPY(from_path, "remote-node-picker-notice-hide-button") }
""",
            from_path="devtools/shared/highlighters.properties",
        ),
    )

    ctx.add_transforms(
        "devtools/shared/highlighters.ftl",
        "devtools/shared/highlighters.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("grid-row-column-positions"),
                value=REPLACE(
                    "devtools/shared/highlighters.properties",
                    "grid.rowColumnPositions",
                    {
                        "%1$S": VARIABLE_REFERENCE("row"),
                        "%2$S": VARIABLE_REFERENCE("column"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("remote-node-picker-notice"),
                value=REPLACE(
                    "devtools/shared/highlighters.properties",
                    "remote-node-picker-notice",
                    {"%1$S": VARIABLE_REFERENCE("action")},
                ),
            ),
        ],
    )
