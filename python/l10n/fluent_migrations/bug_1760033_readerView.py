# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1760033 - Convert AboutReaderParent.jsm to Fluent, part {index}."""

    source = "toolkit/chrome/global/aboutReader.properties"

    ctx.add_transforms(
        "browser/browser/browser.ftl",
        "browser/browser/browser.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("reader-view-enter-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(source, "readerView.enter"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("reader-view-close-button"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("aria-label"),
                        value=COPY(source, "readerView.close"),
                    ),
                ],
            ),
        ],
    )

    ctx.add_transforms(
        "browser/browser/menubar.ftl",
        "browser/browser/menubar.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("menu-view-enter-readerview"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "readerView.enter"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "readerView.enter.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("menu-view-close-readerview"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "readerView.close"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "readerView.close.accesskey"),
                    ),
                ],
            ),
        ],
    )
