# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1552333 - Migrate strings from pipnss.properties to aboutCertError.ftl"""
    ctx.add_transforms(
        "browser/browser/aboutCertError.ftl",
        "browser/browser/aboutCertError.ftl",
        transforms_from(
            """
cert-error-symantec-distrust-admin = { COPY(from_path, "certErrorSymantecDistrustAdministrator") }
""",
            from_path="security/manager/chrome/pipnss/pipnss.properties",
        ),
    )
    ctx.add_transforms(
        "browser/browser/aboutCertError.ftl",
        "browser/browser/aboutCertError.ftl",
        [
            FTL.Message(
                id=FTL.Identifier("cert-error-symantec-distrust-description"),
                value=REPLACE(
                    "security/manager/chrome/pipnss/pipnss.properties",
                    "certErrorSymantecDistrustDescription1",
                    {
                        "%1$S": VARIABLE_REFERENCE("hostname"),
                    },
                    normalize_printf=True,
                ),
            ),
        ],
    )
