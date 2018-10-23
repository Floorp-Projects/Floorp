from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import TERM_REFERENCE
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate import REPLACE
from fluent.migrate import COPY
from fluent.migrate import CONCAT

def migrate(ctx):
    """Bug 1491677 -  Migrate subsection of strings of extensions.dtd, part {index}"""

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutAddons.ftl",
        "toolkit/toolkit/about/aboutAddons.ftl",
        transforms_from(
"""
addons-window =
    .title = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "addons.windowTitle")}
search-header =
    .placeholder = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "search.placeholder3")}
    .searchbuttonlabel = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "search.buttonlabel")}
search-header-shortcut =
    .key = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "search.commandkey")}
loading-label =
    .value = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "loading.label")}
list-empty-installed =
    .value = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "listEmpty.installed.label")}
list-empty-available-updates = 
    .value ={ COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "listEmpty.availableUpdates.label")}
list-empty-recent-updates =
    .value = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "listEmpty.recentUpdates.label")}
list-empty-find-updates =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "listEmpty.findUpdates.label")}
list-empty-button =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "listEmpty.button.label")}
install-addon-from-file =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "installAddonFromFile.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "installAddonFromFile.accesskey")}
help-button = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "helpButton.label")}
tools-menu =
    .tooltiptext = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "toolsMenu.tooltip")}
show-unsigned-extensions-button =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "showUnsignedExtensions.button.label")}
show-all-extensions-button =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "showAllExtensions.button.label")}
debug-addons =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "debugAddons.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "debugAddons.accesskey")}
cmd-show-details =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.showDetails.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.showDetails.accesskey")}
cmd-find-updates =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.findUpdates.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.findUpdates.accesskey")}
cmd-enable-theme =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.enableTheme.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.enableTheme.accesskey")}
cmd-disable-theme = 
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.disableTheme.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.disableTheme.accesskey")}
cmd-install-addon = 
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.installAddon.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.installAddon.accesskey")}
cmd-contribute =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.contribute.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.contribute.accesskey")}
    .tooltiptext = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "cmd.contribute.tooltip")}
discover-title = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "discover.title")}
discover-footer = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "discover.footer", trim:"True")}
detail-version =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.version.label")}
detail-last-updated =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.lastupdated.label")}
detail-contributions-description = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.contributions.description")}
detail-update-type =
    .value = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.updateType")}
detail-update-default = 
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.updateDefault.label")}
    .tooltiptext = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.updateDefault.tooltip")}
detail-update-automatic =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.updateAutomatic.label")}
    .tooltiptext = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.updateAutomatic.tooltip")}
detail-update-manual =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.updateManual.label")}
    .tooltiptext = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.updateManual.tooltip")}
detail-home = 
    .label ={ COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.home")}
detail-repository = 
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.repository")}
detail-check-for-updates =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.checkForUpdates.label")}
    .accesskey = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.checkForUpdates.accesskey")}
    .tooltiptext = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "detail.checkForUpdates.tooltip")}
detail-rating =
    .value = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "rating2.label")}
addon-restart-now =
    .label = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "addon.restartNow.label")}
disabled-unsigned-heading =
    .value = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "disabledUnsigned.heading")}
disabled-unsigned-learn-more = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "disabledUnsigned.learnMore")}
legacy-warning-show-legacy = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "legacyWarning.showLegacy")}
legacy-extensions =
    .value = { COPY("toolkit/chrome/mozapps/extensions/extensions.dtd", "legacyExtensions.title")}
""")
    )

    ctx.add_transforms(
        "toolkit/toolkit/about/aboutAddons.ftl",
        "toolkit/toolkit/about/aboutAddons.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("preferences"),
                value=FTL.Pattern(
                    elements=[
                        FTL.Placeable(
                            expression=FTL.SelectExpression(
                                selector=FTL.CallExpression(
                                    callee=FTL.Function("PLATFORM"),
                                ),
                                variants=[
                                    FTL.Variant(
                                        key=FTL.VariantName("windows"),
                                        default=False,
                                        value=REPLACE(
                                            "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                            "preferencesWin.label",
                                            {
                                                "&brandShortName;" : TERM_REFERENCE("-brand-short-name")
                                            }
                                        )
                                    ),
                                    FTL.Variant(
                                        key=FTL.VariantName("other"),
                                        default=True,
                                        value=REPLACE(
                                            "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                            "preferencesUnix.label",
                                            {
                                                "&brandShortName;" : TERM_REFERENCE("-brand-short-name")
                                            }
                                        )
                                    )
                                ]
                            )
                        )
                    ]
                )
            ),
            FTL.Message(
                id=FTL.Identifier("cmd-preferences"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=FTL.SelectExpression(
                                        selector=FTL.CallExpression(
                                            callee=FTL.Function("PLATFORM"),
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.VariantName("windows"),
                                                default=False,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "cmd.preferencesWin.label"
                                                )
                                            ),
                                            FTL.Variant(
                                                key=FTL.VariantName("other"),
                                                default=True,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "cmd.preferencesUnix.label"
                                                )
                                            )
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
                                            callee=FTL.Function("PLATFORM"),
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.VariantName("windows"),
                                                default=False,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "cmd.preferencesWin.accesskey"
                                                )
                                            ),
                                            FTL.Variant(
                                                key=FTL.VariantName("other"),
                                                default=True,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "cmd.preferencesUnix.accesskey"
                                                )
                                            )
                                        ]
                                    )
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("discover-description"),
                value=REPLACE(
                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                    "discover.description2",
                    {
                        "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                    },
                    trim=True
                )
            ),
            FTL.Message(
                id=FTL.Identifier("detail-home-value"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("detail-home.label")
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("detail-repository-value"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("detail-repository.label")
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("detail-show-preferences"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=FTL.SelectExpression(
                                        selector=FTL.CallExpression(
                                            callee=FTL.Function("PLATFORM"),
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.VariantName("windows"),
                                                default=False,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "detail.showPreferencesWin.label"
                                                )
                                            ),
                                            FTL.Variant(
                                                key=FTL.VariantName("other"),
                                                default=True,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "detail.showPreferencesUnix.label"
                                                )
                                            )
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
                                            callee=FTL.Function("PLATFORM"),
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.VariantName("windows"),
                                                default=False,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "detail.showPreferencesWin.accesskey"
                                                )
                                            ),
                                            FTL.Variant(
                                                key=FTL.VariantName("other"),
                                                default=True,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "detail.showPreferencesUnix.accesskey"
                                                )
                                            )
                                        ]
                                    )
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=FTL.SelectExpression(
                                        selector=FTL.CallExpression(
                                            callee=FTL.Function("PLATFORM"),
                                        ),
                                        variants=[
                                            FTL.Variant(
                                                key=FTL.VariantName("windows"),
                                                default=False,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "detail.showPreferencesWin.tooltip"
                                                )
                                            ),
                                            FTL.Variant(
                                                key=FTL.VariantName("other"),
                                                default=True,
                                                value=COPY(
                                                    "toolkit/chrome/mozapps/extensions/extensions.dtd",
                                                    "detail.showPreferencesUnix.tooltip"
                                                )
                                            )
                                        ]
                                    )
                                )
                            ]
                        )
                    )
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("disabled-unsigned-description"),
                value=CONCAT(
                    REPLACE(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "disabledUnsigned.description.start",
                        {
                            "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                        }
                    ),
                    FTL.TextElement('<label data-l10n-name="find-addons">'),
                    COPY(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "disabledUnsigned.description.findAddonsLink"
                    ),
                    FTL.TextElement("</label>"),
                    COPY(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "disabledUnsigned.description.end"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("disabled-unsigned-devinfo"),
                value=CONCAT(
                    COPY(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "disabledUnsigned.devInfo.start"
                    ),
                    FTL.TextElement('<label data-l10n-name="learn-more">'),
                    COPY(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "disabledUnsigned.devInfo.linkToManual"
                    ),
                    FTL.TextElement("</label>"),
                    COPY(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "disabledUnsigned.devInfo.end"
                    )
                )
            ),
            FTL.Message(
                id=FTL.Identifier("plugin-deprecation-description"),
                value=CONCAT(
                    REPLACE(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "pluginDeprecation.description",
                        {
                            "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                        }
                    ),
                    FTL.TextElement(' <label data-l10n-name="learn-more">'),
                    COPY(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "pluginDeprecation.learnMore"
                    ),
                    FTL.TextElement("</label>")
                )
            ),
            FTL.Message(
                id=FTL.Identifier("legacy-extensions-description"),
                value=CONCAT(
                    REPLACE(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "legacyExtensions.description",
                        {
                            "&brandShortName;": TERM_REFERENCE("-brand-short-name")
                        }
                    ),
                    FTL.TextElement('<label data-l10n-name="legacy-learn-more">'),
                    COPY(
                        "toolkit/chrome/mozapps/extensions/extensions.dtd",
                        "legacyExtensions.learnMore"
                    ),
                    FTL.TextElement("</label>")
                )
            )
        ]
    )
