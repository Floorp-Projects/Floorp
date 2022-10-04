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


class REPLACE_REFERENCES(TransformPattern):
    """Hacky custom transform that replaces one reference."""

    def __init__(self, ctx, path, key, replacements, **kwargs):
        super(REPLACE_REFERENCES, self).__init__(path, key, **kwargs)
        self.ctx = ctx
        self.replacements = replacements

    def visit_Placeable(self, source):
        target = self.generic_visit(source)
        if isinstance(target.expression, FTL.MessageReference):
            expr = target.expression
            for replacement in self.replacements:
                r = MESSAGE_REFERENCE(replacement)
                if expr.id.name != r.id.name:
                    continue
                if expr.attribute.name != r.attribute.name:
                    continue
                target.expression = MESSAGE_REFERENCE(self.replacements[replacement])
                break
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
    """Bug 1792809 - Migrate styles of windows to use min-width/height, part {index}."""
    ctx.add_transforms(
        "browser/browser/places.ftl",
        "browser/browser/places.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/places.ftl",
                "places-library",
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/sanitize.ftl",
        "browser/browser/sanitize.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/sanitize.ftl",
                "sanitize-prefs",
            )
        ],
    )
    ctx.add_transforms(
        "security/manager/security/certificates/certManager.ftl",
        "security/manager/security/certificates/certManager.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "security/manager/security/certificates/certManager.ftl",
                "certmgr-edit-ca-cert",
            ),
            replace_with_min_size_transform(
                ctx,
                "security/manager/security/certificates/certManager.ftl",
                "certmgr-delete-cert",
            ),
        ],
    )
    ctx.add_transforms(
        "security/manager/security/certificates/deviceManager.ftl",
        "security/manager/security/certificates/deviceManager.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "security/manager/security/certificates/deviceManager.ftl",
                "devmgr",
                "devmgr-window",
            )
        ],
    )
    ctx.add_transforms(
        "security/manager/security/pippki/pippki.ftl",
        "security/manager/security/pippki/pippki.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "security/manager/security/pippki/pippki.ftl",
                "reset-primary-password-window",
            ),
            replace_with_min_size_transform(
                ctx,
                "security/manager/security/pippki/pippki.ftl",
                "download-cert-window",
            ),
        ],
    )
    ctx.add_transforms(
        "toolkit/toolkit/global/createProfileWizard.ftl",
        "toolkit/toolkit/global/createProfileWizard.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "toolkit/toolkit/global/createProfileWizard.ftl",
                "create-profile-window",
            ),
            FTL.Message(
                id=FTL.Identifier("create-profile-first-page-header2"),
                value=REPLACE_REFERENCES(
                    ctx,
                    "toolkit/toolkit/global/createProfileWizard.ftl",
                    "create-profile-first-page-header",
                    {
                        "create-profile-window.title": "create-profile-window2.title",
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("create-profile-last-page-header2"),
                value=REPLACE_REFERENCES(
                    ctx,
                    "toolkit/toolkit/global/createProfileWizard.ftl",
                    "create-profile-last-page-header",
                    {
                        "create-profile-window.title": "create-profile-window2.title",
                    },
                ),
            ),
        ],
    )
    ctx.add_transforms(
        "toolkit/toolkit/global/profileDowngrade.ftl",
        "toolkit/toolkit/global/profileDowngrade.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "toolkit/toolkit/global/profileDowngrade.ftl",
                "profiledowngrade-window",
            )
        ],
    )
