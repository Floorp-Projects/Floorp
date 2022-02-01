# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import REPLACE


def migrate(ctx):
    """Bug 1733490 - Convert CSP errors from extensions.properties to Fluent, part {index}."""

    source = "toolkit/chrome/global/extensions.properties"
    target = "toolkit/toolkit/global/cspErrors.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("csp-error-missing-directive"),
                value=REPLACE(
                    source,
                    "csp.error.missing-directive",
                    {"%1$S": VARIABLE_REFERENCE("directive")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("csp-error-illegal-keyword"),
                value=REPLACE(
                    source,
                    "csp.error.illegal-keyword",
                    {
                        "%1$S": VARIABLE_REFERENCE("directive"),
                        "%2$S": VARIABLE_REFERENCE("keyword"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("csp-error-illegal-protocol"),
                value=REPLACE(
                    source,
                    "csp.error.illegal-protocol",
                    {
                        "%1$S": VARIABLE_REFERENCE("directive"),
                        "%2$S": VARIABLE_REFERENCE("scheme"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("csp-error-missing-host"),
                value=REPLACE(
                    source,
                    "csp.error.missing-host",
                    {
                        "%2$S": VARIABLE_REFERENCE("scheme"),
                        "%1$S": VARIABLE_REFERENCE("directive"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("csp-error-missing-source"),
                value=REPLACE(
                    source,
                    "csp.error.missing-source",
                    {
                        "%1$S": VARIABLE_REFERENCE("directive"),
                        "%2$S": VARIABLE_REFERENCE("source"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("csp-error-illegal-host-wildcard"),
                value=REPLACE(
                    source,
                    "csp.error.illegal-host-wildcard",
                    {
                        "%2$S": VARIABLE_REFERENCE("scheme"),
                        "%1$S": VARIABLE_REFERENCE("directive"),
                    },
                ),
            ),
        ],
    )
