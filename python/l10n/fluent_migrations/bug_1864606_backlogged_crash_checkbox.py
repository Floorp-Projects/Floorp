# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import re
import fluent.syntax.ast as FTL
from fluent.migrate.transforms import TransformPattern, COPY_PATTERN


class STRIP_ANCHOR(TransformPattern):
    # Used to remove `<a data-l10n-name="crash-reports-link">[...]</a>` from a string
    def visit_TextElement(self, node):
        node.value = re.sub(
            ' *<a data-l10n-name="crash-reports-link">.*</a>', "", node.value
        )
        return node


def migrate(ctx):
    """Bug 1864606 - Standardize the backlogged crash reports checkbox implementation, part {index}."""
    preferences_ftl = "browser/browser/preferences/preferences.ftl"
    ctx.add_transforms(
        preferences_ftl,
        preferences_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("collection-backlogged-crash-reports"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            preferences_ftl,
                            "collection-backlogged-crash-reports-with-link.accesskey",
                        ),
                    ),
                ],
                value=STRIP_ANCHOR(
                    preferences_ftl, "collection-backlogged-crash-reports-with-link"
                ),
            ),
        ],
    )
