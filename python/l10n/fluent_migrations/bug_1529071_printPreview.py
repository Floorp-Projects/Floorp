# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import MESSAGE_REFERENCE, TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate import COPY, CONCAT, REPLACE


def migrate(ctx):
    """Bug 1529071 - Migrate printPreview to fluent, part {index}"""

    ctx.maybe_add_localization('toolkit/chrome/global/printPreview.dtd')


    ctx.add_transforms(
        'toolkit/toolkit/printing/printPreview.ftl',
        'toolkit/toolkit/printing/printPreview.ftl',
        transforms_from(
"""
printpreview-simplify-page-checkbox = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "simplifyPage.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "simplifyPage.accesskey") }
    .tooltiptext = { COPY("toolkit/chrome/global/printPreview.dtd", "simplifyPage.disabled.tooltip") }
printpreview-close = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "close.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "close.accesskey") }
printpreview-portrait = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "portrait.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "portrait.accesskey") }
printpreview-landscape = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "landscape.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "landscape.accesskey") }
printpreview-scale = 
    .value = { COPY("toolkit/chrome/global/printPreview.dtd", "scale.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "scale.accesskey") }
printpreview-shrink-to-fit = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "ShrinkToFit.label") }
printpreview-custom = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "Custom.label") }
printpreview-print = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "print.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "print.accesskey") }
printpreview-of = 
    .value = { COPY("toolkit/chrome/global/printPreview.dtd", "of.label") }
printpreview-custom-prompt = 
    .value = { COPY("toolkit/chrome/global/printPreview.dtd", "customPrompt.title") }
printpreview-page-setup = 
    .label = { COPY("toolkit/chrome/global/printPreview.dtd", "pageSetup.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "pageSetup.accesskey") }
printpreview-page = 
    .value = { COPY("toolkit/chrome/global/printPreview.dtd", "page.label") }
    .accesskey = { COPY("toolkit/chrome/global/printPreview.dtd", "page.accesskey") }
"""
        )
    ),
    ctx.add_transforms(
        'toolkit/toolkit/printing/printPreview.ftl',
        'toolkit/toolkit/printing/printPreview.ftl',
        [
            FTL.Message(
                id=FTL.Identifier("printpreview-simplify-page-checkbox-enabled"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("printpreview-simplify-page-checkbox.label")
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=MESSAGE_REFERENCE("printpreview-simplify-page-checkbox.accesskey")
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(
                            "toolkit/chrome/global/printPreview.dtd",
                            "simplifyPage.enabled.tooltip"
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("printpreview-homearrow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=VARIABLE_REFERENCE("arrow")
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(
                            "toolkit/chrome/global/printPreview.dtd",
                            "homearrow.tooltip"
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("printpreview-previousarrow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=VARIABLE_REFERENCE("arrow")
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(
                            "toolkit/chrome/global/printPreview.dtd",
                            "previousarrow.tooltip"
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("printpreview-nextarrow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=VARIABLE_REFERENCE("arrow")
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(
                            "toolkit/chrome/global/printPreview.dtd",
                            "nextarrow.tooltip"
                        )
                    ),
                ]
            ),
            FTL.Message(
                id=FTL.Identifier("printpreview-endarrow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=FTL.Pattern(
                            elements=[
                                FTL.Placeable(
                                    expression=VARIABLE_REFERENCE("arrow")
                                )
                            ]
                        )
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("tooltiptext"),
                        value=COPY(
                            "toolkit/chrome/global/printPreview.dtd",
                            "endarrow.tooltip"
                        )
                    ),
                ]
            ),
        ]
        
    )    

