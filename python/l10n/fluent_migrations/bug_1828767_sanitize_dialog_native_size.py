from fluent.migrate import COPY_PATTERN
from fluent.migrate.transforms import TransformPattern, REPLACE_IN_TEXT
from fluent.migrate.helpers import MESSAGE_REFERENCE
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


def replace_with_min_size_transform(ctx, file, identifier, to_identifier=None):
    if to_identifier is None:
        to_identifier = identifier + "2"
    attributes = [
        FTL.Attribute(
            id=FTL.Identifier("title"),
            value=COPY_PATTERN(file, "{}.title".format(identifier)),
        ),
        FTL.Attribute(
            id=FTL.Identifier("style"),
            value=REPLACE_PATTERN(
                ctx,
                file,
                "{}.style".format(identifier),
                {
                    "width:": FTL.TextElement("min-width:"),
                    "height:": FTL.TextElement("min-height:"),
                },
            ),
        ),
    ]

    return FTL.Message(
        id=FTL.Identifier(to_identifier),
        attributes=attributes,
    )


def migrate(ctx):
    """Bug 1828767 - Migrate sanitize dialog to use min-width, part {index}."""
    ctx.add_transforms(
        "browser/browser/sanitize.ftl",
        "browser/browser/sanitize.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/sanitize.ftl",
                "dialog-title",
                "sanitize-dialog-title",
            ),
            replace_with_min_size_transform(
                ctx,
                "browser/browser/sanitize.ftl",
                "dialog-title-everything",
                "sanitize-dialog-title-everything",
            ),
        ],
    )
