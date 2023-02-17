# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
import re
from fluent.migrate.transforms import COPY_PATTERN, TransformPattern


class STRIP_SPAN(TransformPattern):
    def visit_TextElement(self, node):
        node.value = re.sub("</?span[^>]*>", "", node.value)
        return node


def migrate(ctx):
    """Bug 1812694 - Add separate aria-valuetext for videocontrols scrubber, part {index}."""

    path = "toolkit/toolkit/global/videocontrols.ftl"
    ctx.add_transforms(
        path,
        path,
        [
            FTL.Message(
                id=FTL.Identifier("videocontrols-scrubber-position-and-duration"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY_PATTERN(path, "videocontrols-scrubber.aria-label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("aria-valuetext"),
                        value=STRIP_SPAN(
                            path, "videocontrols-position-and-duration-labels"
                        ),
                    ),
                ],
            ),
        ],
    )
