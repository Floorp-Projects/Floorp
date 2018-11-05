# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE, TERM_REFERENCE
from fluent.migrate import REPLACE, COPY


def migrate(ctx):
    """Bug 1486937 - Move strings from preferences.properties to Fluent"""

    ctx.add_transforms(
        "toolkit/toolkit/preferences/preferences.ftl",
        "toolkit/toolkit/preferences/preferences.ftl",
        transforms_from(
            """
password-not-set =
        .value = { COPY(from_path, "password_not_set") }
failed-pw-change = { COPY(from_path, "failed_pw_change") }
incorrect-pw = { COPY(from_path, "incorrect_pw") }
pw-empty-warning = { COPY(from_path, "pw_empty_warning") }
pw-change-ok = { COPY(from_path, "pw_change_ok") } { -pw-empty-warning }
pw-erased-ok = { COPY(from_path, "pw_erased_ok") } { -pw-empty-warning }
pw-not-wanted = { COPY(from_path, "pw_not_wanted") }
pw-change2empty-in-fips-mode = { COPY(from_path, "pw_change2empty_in_fips_mode") }
pw-change-success-title = { COPY(from_path, "pw_change_success_title") }
pw-change-failed-title = { COPY(from_path, "pw_change_failed_title") }
pw-remove-button =
    .label = { COPY(from_path, "pw_remove_button") }
""", from_path="toolkit/chrome/mozapps/preferences/preferences.properties"))

    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        transforms_from(
            """
space-alert-learn-more-button =
        .label = { COPY(from_path, "spaceAlert.learnMoreButton.label")}
        .accesskey = { COPY(from_path, "spaceAlert.learnMoreButton.accesskey")}
space-alert-under-5gb-ok-button =
        .label = { COPY(from_path, "spaceAlert.under5GB.okButton.label")}
        .accesskey = { COPY(from_path, "spaceAlert.under5GB.okButton.accesskey")}
""", from_path="browser/chrome/browser/preferences/preferences.properties"))

    ctx.add_transforms(
        "browser/browser/preferences/preferences.ftl",
        "browser/browser/preferences/preferences.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("space-alert-under-5gb-message"),
                value=REPLACE(
                    "browser/chrome/browser/preferences/preferences.properties",
                    "spaceAlert.under5GB.message",
                    {
                        "%S": TERM_REFERENCE(
                            "-brand-short-name"
                        ),
                    }
                )
            ),
            FTL.Message(
                id=FTL.Identifier("space-alert-over-5gb-pref-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=FTL.SelectExpression(
                                        selector=FTL.CallExpression(
                                            callee=FTL.Function("PLATFORM")
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.VariantName("windows"),
                                                default=False,
                                                value=COPY(
                                                    "browser/chrome/browser/preferences/preferences.properties",
                                                    "spaceAlert.over5GB.prefButtonWin.label"
                                                )
                                            ),
                                            FTL.Variant(
                                                key=FTL.VariantName("other"),
                                                default=True,
                                                value=COPY(
                                                    "browser/chrome/browser/preferences/preferences.properties",
                                                    "spaceAlert.over5GB.prefButton.label"
                                                )
                                            ),
                                        ]
                                    )
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=FTL.SelectExpression(
                                        selector=FTL.CallExpression(
                                            callee=FTL.Function("PLATFORM")
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.VariantName("windows"),
                                                default=False,
                                                value=COPY(
                                                    "browser/chrome/browser/preferences/preferences.properties",
                                                    "spaceAlert.over5GB.prefButtonWin.accesskey"
                                                )
                                            ),
                                            FTL.Variant(
                                                key=FTL.VariantName("other"),
                                                default=True,
                                                value=COPY(
                                                    "browser/chrome/browser/preferences/preferences.properties",
                                                    "spaceAlert.over5GB.prefButton.accesskey"
                                                )
                                            ),
                                        ]
                                    )
                                )
                            ]
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("space-alert-over-5gb-message"),
                value=FTL.Pattern(
                    elements=[
                        FTL.Placeable(
                            expression=FTL.SelectExpression(
                                selector=FTL.CallExpression(
                                    callee=FTL.Function("PLATFORM")
                                ),
                                variants=[
                                    FTL.Variant(
                                        key=FTL.VariantName("windows"),
                                        default=False,
                                        value=REPLACE(
                                            "browser/chrome/browser/preferences/preferences.properties",
                                            "spaceAlert.over5GB.messageWin1",
                                            {
                                                "%S": TERM_REFERENCE(
                                                    "-brand-short-name"
                                                ),
                                            }
                                        )
                                    ),
                                    FTL.Variant(
                                        key=FTL.VariantName("other"),
                                        default=True,
                                        value=REPLACE(
                                            "browser/chrome/browser/preferences/preferences.properties",
                                            "spaceAlert.over5GB.message1",
                                            {
                                                "%S": TERM_REFERENCE(
                                                    "-brand-short-name"
                                                ),
                                            }
                                        )
                                    ),
                                ]
                            )
                        )
                    ]
                )
            ),
        ],
    )
