# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL

from fluent.migrate.transforms import TransformPattern


class STRIP_LEARNMORE(TransformPattern):
    # Used to remove `<a data-l10n-name="link">SOME TEXT</a>` from a string
    def visit_TextElement(self, node):
        link_start = node.value.find('<label data-l10n-name="link">')
        # Replace string up to the link, remove remaining spaces afterwards.
        # Removing an extra character directly is not safe, as it could be
        # punctuation.
        node.value = node.value[:link_start].rstrip()

        return node


def migrate(ctx):
    """Bug 1814266 - Use moz-support-link in identity panel, part {index}."""

    browser_ftl = "browser/browser/browser.ftl"
    ctx.add_transforms(
        browser_ftl,
        browser_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("identity-description-custom-root2"),
                value=STRIP_LEARNMORE(browser_ftl, "identity-description-custom-root"),
            ),
        ],
    )
