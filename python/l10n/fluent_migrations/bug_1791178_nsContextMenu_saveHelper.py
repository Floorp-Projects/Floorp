# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1791178 - Convert nsContextMenu saveHelper() localization to Fluent, part {index}."""

    source = "toolkit/chrome/mozapps/downloads/downloads.properties"
    target = "browser/browser/downloads.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("downloads-error-alert-title"),
                value=COPY(source, "downloadErrorAlertTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("downloads-error-blocked-by"),
                value=REPLACE(
                    source,
                    "downloadErrorBlockedBy",
                    {"%1$S": VARIABLE_REFERENCE("extension")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("downloads-error-extension"),
                value=COPY(source, "downloadErrorExtension"),
            ),
            FTL.Message(
                id=FTL.Identifier("downloads-error-generic"),
                value=COPY(source, "downloadErrorGeneric"),
            ),
        ],
    )
