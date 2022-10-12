# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY_PATTERN


def migrate(ctx):
    """Bug 1794423 - Clean up neterror strings, part {index}."""

    certerror = "toolkit/toolkit/neterror/certError.ftl"
    nsserrors = "toolkit/toolkit/neterror/nsserrors.ftl"

    ctx.add_transforms(
        certerror,
        certerror,
        [
            FTL.Message(
                id=FTL.Identifier("cert-error-ssl-connection-error"),
                value=COPY_PATTERN(nsserrors, "ssl-connection-error"),
            ),
            FTL.Message(
                id=FTL.Identifier("cert-error-code-prefix"),
                value=COPY_PATTERN(nsserrors, "cert-error-code-prefix"),
            ),
        ],
    )
