# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate import COPY
from fluent.migrate import REPLACE
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE
from fluent.migrate.helpers import VARIABLE_REFERENCE

def migrate(ctx):
    """Bug 1523737 - Migrate printProgress.dtd, printPageSetup.dtd, and printPreviewProgress.dtd to Fluent, part {index}."""

    ctx.add_transforms(
        "toolkit/toolkit/printing/printDialogs.ftl",
        "toolkit/toolkit/printing/printDialogs.ftl",
        transforms_from(
"""
print-setup =
    .title = { COPY(from_path, "printSetup.title") }
custom-prompt-title = { COPY(from_path, "customPrompt.title") }
custom-prompt-prompt = { COPY(from_path, "customPrompt.prompt") }
basic-tab =
    .label = { COPY(from_path, "basic.tab") }
advanced-tab =
    .label = { COPY(from_path, "advanced.tab") }
format-group-label =
    .value = { COPY(from_path, "formatGroup.label") }
orientation-label =
    .value = { COPY(from_path, "orientation.label") }
portrait =
    .label = { COPY(from_path, "portrait.label") }
    .accesskey = { COPY(from_path, "portrait.accesskey") }
landscape =
    .label = { COPY(from_path, "landscape.label") }
    .accesskey = { COPY(from_path, "landscape.accesskey") }
scale =
    .label = { COPY(from_path, "scale.label") }
    .accesskey = { COPY(from_path, "scale.accesskey") }
scale-percent =
    .value = { COPY(from_path, "scalePercent") }
shrink-to-fit =
    .label = { COPY(from_path, "shrinkToFit.label") }
    .accesskey = { COPY(from_path, "shrinkToFit.accesskey") }
options-group-label =
    .value = { COPY(from_path, "optionsGroup.label") }
print-bg =
    .label = { COPY(from_path, "printBG.label") }
    .accesskey = { COPY(from_path, "printBG.accesskey") }
margin-top =
    .value = { COPY(from_path, "marginTop.label") }
    .accesskey = { COPY(from_path, "marginTop.accesskey") }
margin-top-invisible =
    .value = { COPY(from_path, "marginTop.label") }
margin-bottom =
    .value = { COPY(from_path, "marginBottom.label") }
    .accesskey = { COPY(from_path, "marginBottom.accesskey") }
margin-bottom-invisible =
    .value = { COPY(from_path, "marginBottom.label") }
margin-left =
    .value = { COPY(from_path, "marginLeft.label") }
    .accesskey = { COPY(from_path, "marginLeft.accesskey") }
margin-left-invisible =
    .value = { COPY(from_path, "marginLeft.label") }
margin-right =
    .value = { COPY(from_path, "marginRight.label") }
    .accesskey = { COPY(from_path, "marginRight.accesskey") }
margin-right-invisible =
    .value = { COPY(from_path, "marginRight.label") }
header-footer-label =
    .value = { COPY(from_path, "headerFooter.label") }
hf-left-label =
    .value = { COPY(from_path, "hfLeft.label") }
hf-center-label =
    .value = { COPY(from_path, "hfCenter.label") }
hf-right-label =
    .value = { COPY(from_path, "hfRight.label") }
header-left-tip =
    .tooltiptext = { COPY(from_path, "headerLeft.tip") }
header-center-tip =
    .tooltiptext = { COPY(from_path, "headerCenter.tip") }
header-right-tip =
    .tooltiptext = { COPY(from_path, "headerRight.tip") }
footer-left-tip =
    .tooltiptext = { COPY(from_path, "footerLeft.tip") }
footer-center-tip =
    .tooltiptext = { COPY(from_path, "footerCenter.tip") }
footer-right-tip =
    .tooltiptext = { COPY(from_path, "footerRight.tip") }
hf-blank =
    .label = { COPY(from_path, "hfBlank") }
hf-title =
    .label = { COPY(from_path, "hfTitle") }
hf-url =
    .label = { COPY(from_path, "hfURL") }
hf-date-and-time =
    .label = { COPY(from_path, "hfDateAndTime") }
hf-page =
    .label = { COPY(from_path, "hfPage") }
hf-page-and-total =
    .label = { COPY(from_path, "hfPageAndTotal") }
hf-custom =
    .label = { COPY(from_path, "hfCustom") }
""", from_path="toolkit/chrome/global/printPageSetup.dtd")
    )

    ctx.add_transforms(
        "toolkit/toolkit/printing/printDialogs.ftl",
        "toolkit/toolkit/printing/printDialogs.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("margin-group-label-inches"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=REPLACE(
                            "toolkit/chrome/global/printPageSetup.dtd",
                            "marginGroup.label",
                            {
                                "#1": COPY("toolkit/chrome/global/printPageSetup.dtd", "marginUnits.inches")
                            },
                        )
                    ),
                ]
            )
        ]
    )

    ctx.add_transforms(
        "toolkit/toolkit/printing/printDialogs.ftl",
        "toolkit/toolkit/printing/printDialogs.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("margin-group-label-metric"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=REPLACE(
                            "toolkit/chrome/global/printPageSetup.dtd",
                            "marginGroup.label",
                            {
                                "#1": COPY("toolkit/chrome/global/printPageSetup.dtd", "marginUnits.metric")
                            },
                        )
                    ),
                ]
            )
        ]
    )

    ctx.add_transforms(
        "toolkit/toolkit/printing/printDialogs.ftl",
        "toolkit/toolkit/printing/printDialogs.ftl",
        transforms_from(
"""
print-preview-window =
    .title = { COPY(from_path, "printWindow.title") }
print-title =
    .value = { COPY(from_path, "title") }
print-preparing =
    .value = { COPY(from_path, "preparing") }
print-progress =
    .value = { COPY(from_path, "progress") }
""", from_path="toolkit/chrome/global/printPreviewProgress.dtd")
    )

    ctx.add_transforms(
        "toolkit/toolkit/printing/printDialogs.ftl",
        "toolkit/toolkit/printing/printDialogs.ftl",
        transforms_from(
"""
print-window =
    .title = { COPY(from_path, "printWindow.title") }
print-complete =
    .value = { COPY(from_path, "printComplete") }
dialog-cancel-label = { COPY(from_path, "dialogCancel.label") }
dialog-close-label = { COPY(from_path, "dialogClose.label") }
""", from_path="toolkit/chrome/global/printProgress.dtd")
    )

    ctx.add_transforms(
        "toolkit/toolkit/printing/printDialogs.ftl",
        "toolkit/toolkit/printing/printDialogs.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("print-percent"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("value"),
                        value=REPLACE(
                            "toolkit/chrome/global/printProgress.dtd",
                            "percentPrint",
                            {
                                "#1": VARIABLE_REFERENCE("percent")
                            },
                        )
                    ),
                ]
            )
        ]
    )
