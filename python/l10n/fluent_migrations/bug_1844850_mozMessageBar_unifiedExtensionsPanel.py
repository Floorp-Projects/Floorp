# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL

from fluent.migrate import COPY_PATTERN


def migrate(ctx):
    """Bug 1844850 - Use moz-message-bar in the unified extensions panel, part {index}."""
    unifiedExtensions_ftl = "browser/browser/unifiedExtensions.ftl"
    ctx.add_transforms(
        unifiedExtensions_ftl,
        unifiedExtensions_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("unified-extensions-mb-quarantined-domain-message-3"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("heading"),
                        value=COPY_PATTERN(
                            unifiedExtensions_ftl,
                            "unified-extensions-mb-quarantined-domain-title",
                        ),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("message"),
                        value=COPY_PATTERN(
                            unifiedExtensions_ftl,
                            "unified-extensions-mb-quarantined-domain-message-2",
                        ),
                    ),
                ],
            ),
        ],
    )
