# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL

from fluent.migrate.transforms import TransformPattern


class STRIP_LEARNMORE(TransformPattern):
    # Used to remove `<a data-l10n-name="deprecation-link">SOME TEXT</a>` from a string
    def visit_TextElement(self, node):
        link_start = node.value.find('<a data-l10n-name="deprecation-link">')
        if link_start != -1:
            # Replace string up to the link, remove remaining spaces afterwards.
            # Removing an extra character directly is not safe, as it could be
            # punctuation.
            node.value = node.value[:link_start].rstrip()

        return node


def migrate(ctx):
    """Bug 1844848 - Use moz-message-bar in about:plugins , part {index}."""
    abouPlugins_ftl = "toolkit/toolkit/about/aboutPlugins.ftl"
    ctx.add_transforms(
        abouPlugins_ftl,
        abouPlugins_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("deprecation-description2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_LEARNMORE(
                            abouPlugins_ftl,
                            "deprecation-description",
                        ),
                    ),
                ],
            ),
        ],
    )
