# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1508156 - Use fluent for the default application choice section of about:preferences, part {index}"""

    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        transforms_from(
            """
applications-select-helper = { COPY(from_path, "fpTitleChooseApp") }
applications-manage-app =
    .label = { COPY(from_path, "manageApp") }
applications-type-pdf = { COPY(from_path, "portableDocumentFormat") }
applications-type-pdf-with-type = { applications-type-pdf } ({ $type })
applications-type-description-with-type = { $type-description } ({ $type })
applications-action-save =
    .label = { COPY(from_path, "saveFile") }

applications-always-ask =
    .label = { COPY(from_path, "alwaysAsk") }

applications-use-other =
    .label = { COPY(from_path, "useOtherApp") }

applications-use-plugin-in-label =
    .value = { applications-use-plugin-in.label }

applications-action-save-label =
    .value = { applications-action-save.label }

applications-use-app-label =
    .value = { applications-use-app.label }

applications-preview-inapp-label =
    .value = { applications-preview-inapp.label }

applications-always-ask-label =
    .value = { applications-always-ask.label }

applications-use-app-default-label =
    .value = { applications-use-app-default.label }

applications-use-other-label =
    .value = { applications-use-other.label }

""", from_path="browser/chrome/browser/preferences/preferences.properties"))

    from_path = "browser/chrome/browser/preferences/preferences.properties"
    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("applications-file-ending"),
                value=REPLACE(
                    from_path,
                    "fileEnding",
                    {
                        "%1$S": VARIABLE_REFERENCE(
                            "extension"
                        ),
                    },
                    normalize_printf=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("applications-use-plugin-in"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            from_path,
                            "usePluginIn",
                            {
                                "%1$S": VARIABLE_REFERENCE(
                                    "plugin-name"
                                ),
                                "%2$S": TERM_REFERENCE(
                                    "brand-short-name"
                                ),
                            },
                            normalize_printf=True
                        ),
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("applications-use-app"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            from_path,
                            "useApp",
                            {
                                "%1$S": VARIABLE_REFERENCE(
                                    "app-name"
                                ),
                            },
                            normalize_printf=True
                        ),
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("applications-preview-inapp"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            from_path,
                            "previewInApp",
                            {
                                "%1$S": TERM_REFERENCE(
                                    "brand-short-name"
                                ),
                            },
                            normalize_printf=True
                        ),
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("applications-use-app-default"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=REPLACE(
                            from_path,
                            "useDefault",
                            {
                                "%1$S": VARIABLE_REFERENCE(
                                    "app-name"
                                ),
                            },
                            normalize_printf=True
                        ),
                    ),
                ]
            ),
        ]
    )
