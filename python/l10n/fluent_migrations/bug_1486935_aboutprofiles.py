from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import REPLACE
from fluent.migrate import COPY


def migrate(ctx):
    """ Bug 1486935 - Migrate about:Profiles strings to FTL, part {index}. """

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutProfiles.ftl",
        "toolkit/toolkit/about/aboutProfiles.ftl",
         transforms_from(
"""
profiles-title = { COPY("toolkit/chrome/global/aboutProfiles.dtd", "aboutProfiles.title")}
profiles-subtitle = { COPY("toolkit/chrome/global/aboutProfiles.dtd", "aboutProfiles.subtitle")}
profiles-create = { COPY("toolkit/chrome/global/aboutProfiles.dtd", "aboutProfiles.create")}
profiles-restart-title = { COPY("toolkit/chrome/global/aboutProfiles.dtd", "aboutProfiles.restart.title")}
profiles-restart-in-safe-mode = { COPY("toolkit/chrome/global/aboutProfiles.dtd", "aboutProfiles.restart.inSafeMode")}
profiles-restart-normal = { COPY("toolkit/chrome/global/aboutProfiles.dtd", "aboutProfiles.restart.normal")}

profiles-is-default = { COPY("toolkit/chrome/global/aboutProfiles.properties", "isDefault")}
profiles-rootdir = { COPY("toolkit/chrome/global/aboutProfiles.properties", "rootDir")}
profiles-localdir = { COPY("toolkit/chrome/global/aboutProfiles.properties", "localDir")}
profiles-current-profile = { COPY("toolkit/chrome/global/aboutProfiles.properties", "currentProfile")}
profiles-in-use-profile = { COPY("toolkit/chrome/global/aboutProfiles.properties", "inUseProfile")}
profiles-rename = { COPY("toolkit/chrome/global/aboutProfiles.properties", "rename")}
profiles-remove = { COPY("toolkit/chrome/global/aboutProfiles.properties", "remove")}
profiles-set-as-default = { COPY("toolkit/chrome/global/aboutProfiles.properties", "setAsDefault")}
profiles-launch-profile = { COPY("toolkit/chrome/global/aboutProfiles.properties", "launchProfile")}
profiles-yes = { COPY("toolkit/chrome/global/aboutProfiles.properties", "yes")}
profiles-no = { COPY("toolkit/chrome/global/aboutProfiles.properties", "no")}
profiles-rename-profile-title = { COPY("toolkit/chrome/global/aboutProfiles.properties", "renameProfileTitle")}
profiles-invalid-profile-name-title = { COPY("toolkit/chrome/global/aboutProfiles.properties", "invalidProfileNameTitle")}
profiles-delete-profile-title = { COPY("toolkit/chrome/global/aboutProfiles.properties", "deleteProfileTitle")}
profiles-delete-files = { COPY("toolkit/chrome/global/aboutProfiles.properties", "deleteFiles")}
profiles-dont-delete-files = { COPY("toolkit/chrome/global/aboutProfiles.properties", "dontDeleteFiles")}
profiles-delete-profile-failed-title = { COPY("toolkit/chrome/global/aboutProfiles.properties", "deleteProfileFailedTitle")}
profiles-delete-profile-failed-message = { COPY("toolkit/chrome/global/aboutProfiles.properties", "deleteProfileFailedMessage")}
""")
    )

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutProfiles.ftl",
        "toolkit/toolkit/about/aboutProfiles.ftl",
        [
            FTL.Message(
            id=FTL.Identifier("profiles-name"),
            value=REPLACE(
                "toolkit/chrome/global/aboutProfiles.properties",
                "name",
                {
                    "%S": VARIABLE_REFERENCE(
                        "name"
                    ),
                }
            )
        ),
            FTL.Message(
            id=FTL.Identifier("profiles-rename-profile"),
            value=REPLACE(
                "toolkit/chrome/global/aboutProfiles.properties",
                "renameProfile",
                {
                    "%S": VARIABLE_REFERENCE(
                        "name"
                    ),
                }
            )
        ),
            FTL.Message(
            id=FTL.Identifier("profiles-invalid-profile-name"),
            value=REPLACE(
                "toolkit/chrome/global/aboutProfiles.properties",
                "invalidProfileName",
                {
                    "%S": VARIABLE_REFERENCE(
                        "name"
                    ),
                }
            )
        ),
            FTL.Message(
            id=FTL.Identifier("profiles-delete-profile-confirm"),
            value=REPLACE(
                "toolkit/chrome/global/aboutProfiles.properties",
                "deleteProfileConfirm",
                {
                    "%S": VARIABLE_REFERENCE(
                        "dir"
                    ),
                }
            )
        ),
        FTL.Message(
            id=FTL.Identifier("profiles-opendir"),
            value=FTL.Pattern(
                elements=[
                    FTL.Placeable(
                        expression=FTL.SelectExpression(
                            selector=FTL.CallExpression(
                                callee=FTL.Function("PLATFORM")
                            ),
                            variants=[
                                FTL.Variant(
                                    key=FTL.VariantName("macos"),
                                    default=False,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutProfiles.properties",
                                        "macOpenDir"
                                    )
                                ),
                                FTL.Variant(
                                    key=FTL.VariantName("windows"),
                                    default=False,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutProfiles.properties",
                                        "winOpenDir2"
                                    )
                                ),
                                FTL.Variant(
                                    key=FTL.VariantName("other"),
                                    default=True,
                                    value=COPY(
                                        "toolkit/chrome/global/aboutProfiles.properties",
                                        "openDir"
                                    )
                                )
                            ]
                        )
                    )
                ]
            )
        )
    ]
)
