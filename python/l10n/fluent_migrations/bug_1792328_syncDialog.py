from fluent.migrate import COPY_PATTERN
from fluent.migrate.helpers import transforms_from
from fluent.migrate.transforms import TransformPattern, REPLACE_IN_TEXT
import fluent.syntax.ast as FTL

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/


class REPLACE_PATTERN(TransformPattern):
    """Hacky custom transform that works."""

    def __init__(self, ctx, path, key, replacements, **kwargs):
        super(REPLACE_PATTERN, self).__init__(path, key, **kwargs)
        self.ctx = ctx
        self.replacements = replacements

    def visit_Pattern(self, source):
        source = self.generic_visit(source)
        target = FTL.Pattern([])
        for element in source.elements:
            if isinstance(element, FTL.TextElement):
                pattern = REPLACE_IN_TEXT(element, self.replacements)(self.ctx)
                target.elements += pattern.elements
            else:
                target.elements += [element]
        return target


def migrate(ctx):
    """Bug 1792328 - Remove min-height from choose what to sync dialog, part {index}"""

    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("sync-choose-what-to-sync-dialog3"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY_PATTERN(
                            "browser/browser/preferences/preferences.ftl",
                            "sync-choose-what-to-sync-dialog2.title",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("style"),
                        value=REPLACE_PATTERN(
                            ctx,
                            "browser/browser/preferences/preferences.ftl",
                            "sync-choose-what-to-sync-dialog2.style",
                            {
                                " min-height: 35em;": FTL.TextElement(""),
                            },
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("buttonlabelaccept"),
                        value=COPY_PATTERN(
                            "browser/browser/preferences/preferences.ftl",
                            "sync-choose-what-to-sync-dialog2.buttonlabelaccept",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("buttonaccesskeyaccept"),
                        value=COPY_PATTERN(
                            "browser/browser/preferences/preferences.ftl",
                            "sync-choose-what-to-sync-dialog2.buttonaccesskeyaccept",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("buttonlabelextra2"),
                        value=COPY_PATTERN(
                            "browser/browser/preferences/preferences.ftl",
                            "sync-choose-what-to-sync-dialog2.buttonlabelextra2",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("buttonaccesskeyextra2"),
                        value=COPY_PATTERN(
                            "browser/browser/preferences/preferences.ftl",
                            "sync-choose-what-to-sync-dialog2.buttonaccesskeyextra2",
                        ),
                    ),
                ],
            ),
        ],
    )
