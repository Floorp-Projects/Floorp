# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1760033 - Convert aboutReader.html to Fluent, part {index}."""

    source = "toolkit/chrome/global/aboutReader.properties"
    target = "toolkit/toolkit/about/aboutReader.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("about-reader-loading"),
                value=COPY(source, "aboutReader.loading2"),
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-load-error"),
                value=COPY(source, "aboutReader.loadError"),
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-color-scheme-light"),
                value=COPY(source, "aboutReader.colorScheme.light"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.colorschemelight"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-color-scheme-dark"),
                value=COPY(source, "aboutReader.colorScheme.dark"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.colorschemedark"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-color-scheme-sepia"),
                value=COPY(source, "aboutReader.colorScheme.sepia"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.colorschemesepia"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-color-scheme-auto"),
                value=COPY(source, "aboutReader.colorScheme.auto"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.colorschemeauto"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-estimated-read-time"),
                value=PLURALS(
                    source,
                    "aboutReader.estimatedReadTimeRange1",
                    VARIABLE_REFERENCE("rangePlural"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            # most locales
                            "#1-#2": VARIABLE_REFERENCE("range"),
                            # bg
                            "#1 – #2": VARIABLE_REFERENCE("range"),
                            # bo
                            "#1་ནས་#2": VARIABLE_REFERENCE("range"),
                            # ja ja-JP-mac
                            "#1 ～ #2": VARIABLE_REFERENCE("range"),
                            # kab
                            "#1 -#2": VARIABLE_REFERENCE("range"),
                            # pl sk
                            "#1 — #2": VARIABLE_REFERENCE("range"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-font-type-serif"),
                value=COPY(source, "aboutReader.fontType.serif"),
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-font-type-sans-serif"),
                value=COPY(source, "aboutReader.fontType.sans-serif"),
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-minus"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.minus"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-plus"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.plus"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-contentwidthminus"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.contentwidthminus"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-contentwidthplus"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.contentwidthplus"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-lineheightminus"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.lineheightminus"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-lineheightplus"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("title"),
                        value=COPY(source, "aboutReader.toolbar.lineheightplus"),
                    )
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-close"),
                value=COPY(source, "aboutReader.toolbar.close"),
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-type-controls"),
                value=COPY(source, "aboutReader.toolbar.typeControls"),
            ),
            FTL.Message(
                id=FTL.Identifier("about-reader-toolbar-savetopocket"),
                value=REPLACE(
                    source,
                    "readerView.savetopocket.label",
                    {"%1$S": TERM_REFERENCE("pocket-brand-name")},
                ),
            ),
        ],
    )
