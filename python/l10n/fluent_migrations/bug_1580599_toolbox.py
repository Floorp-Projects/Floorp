# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1580599 - Convert toolbox.properties to Fluent, part {index}."""

    source = "devtools/client/toolbox.properties"
    target = "devtools/client/toolbox.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-dock-bottom-label"),
                value=COPY(source, "toolbox.meatballMenu.dock.bottom.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-dock-left-label"),
                value=COPY(source, "toolbox.meatballMenu.dock.left.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-dock-right-label"),
                value=COPY(source, "toolbox.meatballMenu.dock.right.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-dock-separate-window-label"),
                value=COPY(source, "toolbox.meatballMenu.dock.separateWindow.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-splitconsole-label"),
                value=COPY(source, "toolbox.meatballMenu.splitconsole.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-hideconsole-label"),
                value=COPY(source, "toolbox.meatballMenu.hideconsole.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-noautohide-label"),
                value=COPY(source, "toolbox.meatballMenu.noautohide.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-settings-label"),
                value=COPY(source, "toolbox.meatballMenu.settings.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-documentation-label"),
                value=COPY(source, "toolbox.meatballMenu.documentation.label"),
            ),
            FTL.Message(
                id=FTL.Identifier("toolbox-meatball-menu-community-label"),
                value=COPY(source, "toolbox.meatballMenu.community.label"),
            ),
        ],
    )
