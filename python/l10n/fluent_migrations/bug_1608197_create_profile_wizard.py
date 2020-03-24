# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate import COPY, REPLACE
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE

def migrate(ctx):
    """Bug 1608197 - Migrate createProfileWizard to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/global/createProfileWizard.ftl",
        "toolkit/toolkit/global/createProfileWizard.ftl",
    transforms_from(
"""
create-profile-window =
    .title = { COPY(from_path, "newprofile.title") }
    .style = { COPY(from_path, "window.size") }
profile-creation-explanation-4 = { PLATFORM() ->
    [macos] { COPY(from_path, "profileCreationExplanation_4Mac.text") }
   *[other] { COPY(from_path, "profileCreationExplanation_4.text") }
  }
profile-creation-intro = { COPY(from_path, "profileCreationIntro.text") }
profile-prompt = { COPY(from_path, "profilePrompt.label") }
    .accesskey = { COPY(from_path, "profilePrompt.accesskey") }
profile-default-name =
    .value = { COPY(from_path, "profileDefaultName") }
profile-directory-explanation = { COPY(from_path, "profileDirectoryExplanation.text") }
create-profile-choose-folder =
    .label = { COPY(from_path, "button.choosefolder.label") }
    .accesskey = { COPY(from_path, "button.choosefolder.accesskey") }
create-profile-use-default =
    .label = { COPY(from_path, "button.usedefault.label") }
    .accesskey = { COPY(from_path, "button.usedefault.accesskey") }
""", from_path="toolkit/chrome/mozapps/profile/createProfileWizard.dtd"))
    ctx.add_transforms(
    "toolkit/toolkit/global/createProfileWizard.ftl",
    "toolkit/toolkit/global/createProfileWizard.ftl",
    [
    FTL.Message(
        id=FTL.Identifier("create-profile-first-page-header"),
        value=FTL.Pattern(
            elements=[
                FTL.Placeable(
                    expression=FTL.SelectExpression(
                        selector=FTL.FunctionReference(
                            id=FTL.Identifier(name="PLATFORM"),
                            arguments=FTL.CallArguments(positional=[], named=[])
                        ),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier('macos'),
                                default=False,
                                value=COPY(
                                    "toolkit/chrome/global/wizard.properties",
                                    "default-first-title-mac",
                                )
                            ),
                            FTL.Variant(
                                key=FTL.Identifier('other'),
                                default=True,
                                value=REPLACE(
                                    "toolkit/chrome/global/wizard.properties",
                                    "default-first-title",
                                    {
                                        "%1$S": MESSAGE_REFERENCE("create-profile-window.title"),
                                    },
                                    normalize_printf=True
                                )
                            )
                        ]
                    )
                )
            ]
        )
    ),
    FTL.Message(
        id=FTL.Identifier("profile-creation-explanation-1"),
        value=REPLACE(
            "toolkit/chrome/mozapps/profile/createProfileWizard.dtd",
            "profileCreationExplanation_1.text",
            {
                "&brandShortName;": TERM_REFERENCE("brand-short-name")
            }
        )
    ),
    FTL.Message(
        id=FTL.Identifier("profile-creation-explanation-2"),
        value=REPLACE(
            "toolkit/chrome/mozapps/profile/createProfileWizard.dtd",
            "profileCreationExplanation_2.text",
            {
                "&brandShortName;": TERM_REFERENCE("brand-short-name")
            }
        )
    ),
    FTL.Message(
        id=FTL.Identifier("profile-creation-explanation-3"),
        value=REPLACE(
            "toolkit/chrome/mozapps/profile/createProfileWizard.dtd",
            "profileCreationExplanation_3.text",
            {
                "&brandShortName;": TERM_REFERENCE("brand-short-name")
            }
        )
    ),
    FTL.Message(
        id=FTL.Identifier("create-profile-last-page-header"),
        value=FTL.Pattern(
            elements=[
                FTL.Placeable(
                    expression=FTL.SelectExpression(
                        selector=FTL.FunctionReference(
                            id=FTL.Identifier(name="PLATFORM"),
                            arguments=FTL.CallArguments(positional=[], named=[])
                        ),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier('macos'),
                                default=False,
                                value=COPY(
                                    "toolkit/chrome/global/wizard.properties",
                                    "default-last-title-mac",
                                )
                            ),
                            FTL.Variant(
                                key=FTL.Identifier('other'),
                                default=True,
                                value=REPLACE(
                                    "toolkit/chrome/global/wizard.properties",
                                    "default-last-title",
                                    {
                                        "%1$S": MESSAGE_REFERENCE("create-profile-window.title"),
                                    },
                                    normalize_printf=True
                                )
                            )
                        ]
                    )
                )
            ]
        )
    ),
    ]
    )
