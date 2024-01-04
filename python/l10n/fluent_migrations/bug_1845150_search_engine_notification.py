# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import re
import fluent.syntax.ast as FTL
from fluent.migrate.transforms import TransformPattern


class STRIP_LABEL(TransformPattern):
    # Used to remove `<label data-l10n-name="remove-search-engine-article">` from a string
    def visit_TextElement(self, node):
        node.value = re.sub(
            '\s?<label data-l10n-name="remove-search-engine-article">.+?</label>\s?',
            "",
            node.value,
        )
        return node


def migrate(ctx):
    """Bug 1845150 - Use moz-message-bar instead of message-bar in notificationbox.js, part {index}."""
    search_ftl = "browser/browser/search.ftl"
    ctx.add_transforms(
        search_ftl,
        search_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("removed-search-engine-message2"),
                value=STRIP_LABEL(search_ftl, "removed-search-engine-message"),
            ),
        ],
    )
