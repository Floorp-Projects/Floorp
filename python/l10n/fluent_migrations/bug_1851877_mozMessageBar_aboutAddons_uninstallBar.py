# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import TransformPattern


class STRIP_SPAN(TransformPattern):
    # Used to remove `<span data-l10n-name="addon-name"></span>` from a string
    def visit_TextElement(self, node):
        span_start = node.value.find('<span data-l10n-name="addon-name">')
        span_end = node.value.find("</span>")
        if span_start != -1 and span_end == -1:
            node.value = node.value[:span_start]
        elif span_start == -1 and span_end != -1:
            node.value = node.value[span_end + 7 :]

        return node


def migrate(ctx):
    """Bug 1851877 - Use moz-message-bar to replace the pending uninstall bar in about:addons, part {index}."""
    aboutAddons_ftl = "toolkit/toolkit/about/aboutAddons.ftl"
    ctx.add_transforms(
        aboutAddons_ftl,
        aboutAddons_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("pending-uninstall-description2"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=STRIP_SPAN(
                            aboutAddons_ftl,
                            "pending-uninstall-description",
                        ),
                    ),
                ],
            ),
        ],
    )
