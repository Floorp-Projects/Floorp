# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY, COPY_PATTERN


def migrate(ctx):
    """Bug 1814969 - Convert contextual identity service strings to Fluent, part {index}."""

    source = "browser/chrome/browser/browser.properties"
    alltabs = "browser/browser/allTabsMenu.ftl"
    target = "toolkit/toolkit/global/contextual-identity.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("user-context-personal"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "userContextPersonal.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "userContextPersonal.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("user-context-work"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "userContextWork.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "userContextWork.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("user-context-banking"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "userContextBanking.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "userContextBanking.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("user-context-shopping"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "userContextShopping.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "userContextShopping.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("user-context-none"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "userContextNone.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "userContextNone.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("user-context-manage-containers"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY_PATTERN(
                            alltabs, "all-tabs-menu-manage-user-context.label"
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY_PATTERN(
                            alltabs, "all-tabs-menu-manage-user-context.accesskey"
                        ),
                    ),
                ],
            ),
        ],
    )
