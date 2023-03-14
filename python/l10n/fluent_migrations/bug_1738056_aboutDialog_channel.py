# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
import re

from fluent.migrate.transforms import TransformPattern


class INSERT_VARIABLE(TransformPattern):
    def visit_TextElement(self, node):
        node.value = re.sub(
            'current-channel"></label',
            'current-channel">{ $channel }</label',
            node.value,
        )
        return node


def migrate(ctx):
    """Bug 1738056 - Convert about dialog channel listing to fluent, part {index}."""

    about_dialog_ftl = "browser/browser/aboutDialog.ftl"
    ctx.add_transforms(
        about_dialog_ftl,
        about_dialog_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("aboutdialog-channel-description"),
                value=INSERT_VARIABLE(about_dialog_ftl, "channel-description"),
            ),
        ],
    )
