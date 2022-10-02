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


def replace_with_min_size_transform(ctx, file, identifier, with_title=True):
    attributes = []
    if with_title:
        attributes.append(
            FTL.Attribute(
                id=FTL.Identifier("title"),
                value=COPY_PATTERN(file, "{}.title".format(identifier)),
            )
        )
    attributes.append(
        FTL.Attribute(
            id=FTL.Identifier("style"),
            value=REPLACE_PATTERN(
                ctx,
                file,
                "{}.style".format(identifier),
                {
                    "width:": FTL.TextElement("min-width:"),
                },
            ),
        )
    )

    return FTL.Message(
        id=FTL.Identifier(identifier + "2"),
        attributes=attributes,
    )


def migrate(ctx):
    """Bug 1792730 - Migrate styles of dialogs to use min-width"""
    for window in [
        "exceptions-etp",
        "exceptions-cookie",
        "exceptions-https-only",
        "exceptions-popup",
        "exceptions-saved-logins",
        "exceptions-addons",
        "site-autoplay",
        "site-notification",
        "site-location",
        "site-xr",
        "site-camera",
        "site-microphone",
    ]:
        ctx.add_transforms(
            "browser/browser/preferences/permissions.ftl",
            "browser/browser/preferences/permissions.ftl",
            transforms_from(
                """
permissions-{0}-window2 =
    .title = {{COPY_PATTERN(from_path, "permissions-{0}-window.title")}}
    .style = {{permissions-window2.style}}
                """.format(
                    window
                ),
                from_path="browser/browser/preferences/permissions.ftl",
            ),
        )

    # It'd be great to not duplicate this using something like the following.
    # for (file, identifier) in [
    #     ("browser/browser/preferences/addEngine.ftl", "add-engine-window"),
    #     ...
    # ]:
    #
    # But the test migration stuff chokes on it and fails.
    ctx.add_transforms(
        "browser/browser/preferences/addEngine.ftl",
        "browser/browser/preferences/addEngine.ftl",
        [
            replace_with_min_size_transform(
                ctx, "browser/browser/preferences/addEngine.ftl", "add-engine-window"
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/applicationManager.ftl",
        "browser/browser/preferences/applicationManager.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/preferences/applicationManager.ftl",
                "app-manager-window",
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/clearSiteData.ftl",
        "browser/browser/preferences/clearSiteData.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/preferences/clearSiteData.ftl",
                "clear-site-data-window",
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/colors.ftl",
        "browser/browser/preferences/colors.ftl",
        [
            replace_with_min_size_transform(
                ctx, "browser/browser/preferences/colors.ftl", "colors-dialog"
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/blocklists.ftl",
        "browser/browser/preferences/blocklists.ftl",
        [
            replace_with_min_size_transform(
                ctx, "browser/browser/preferences/blocklists.ftl", "blocklist-window"
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/blocklists.ftl",
        "browser/browser/preferences/blocklists.ftl",
        [
            replace_with_min_size_transform(
                ctx, "browser/browser/preferences/blocklists.ftl", "blocklist-window"
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/connection.ftl",
        "browser/browser/preferences/connection.ftl",
        [
            replace_with_min_size_transform(
                ctx, "browser/browser/preferences/connection.ftl", "connection-window"
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/containers.ftl",
        "browser/browser/preferences/containers.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/preferences/containers.ftl",
                "containers-window-new",
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/containers.ftl",
        "browser/browser/preferences/containers.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/preferences/containers.ftl",
                "containers-window-update-settings",
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/fxaPairDevice.ftl",
        "browser/browser/preferences/fxaPairDevice.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/preferences/fxaPairDevice.ftl",
                "fxa-pair-device-dialog-sync",
                with_title=False,
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/languages.ftl",
        "browser/browser/preferences/languages.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/preferences/languages.ftl",
                "webpage-languages-window",
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/languages.ftl",
        "browser/browser/preferences/languages.ftl",
        [
            replace_with_min_size_transform(
                ctx,
                "browser/browser/preferences/languages.ftl",
                "browser-languages-window",
            )
        ],
    )
    ctx.add_transforms(
        "browser/browser/preferences/permissions.ftl",
        "browser/browser/preferences/permissions.ftl",
        [
            replace_with_min_size_transform(
                ctx, "browser/browser/preferences/permissions.ftl", "permissions-window"
            )
        ],
    )
