# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, VARIABLE_REFERENCE
from fluent.migrate.transforms import REPLACE


def migrate(ctx):
    """Bug 1831872 - Migrate sync.properties to Fluent, part {index}."""

    source = "services/sync/sync.properties"
    target = "toolkit/services/accounts.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("account-client-name"),
                value=REPLACE(
                    source,
                    "client.name2",
                    {
                        "%1$S": VARIABLE_REFERENCE("user"),
                        "%2$S": TERM_REFERENCE("brand-short-name"),
                        "%3$S": VARIABLE_REFERENCE("system"),
                    },
                ),
            ),
        ],
    )
