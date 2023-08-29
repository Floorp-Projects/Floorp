# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate import COPY, REPLACE


def migrate(ctx):
    """Bug 1401466 - Migrate some client authentication dialog strings from pippki.properties to pippki.ftl, part {index}"""

    pippki_ftl = "security/manager/security/pippki/pippki.ftl"
    pippki_properties = "security/manager/chrome/pippki/pippki.properties"

    ctx.add_transforms(
        pippki_ftl,
        pippki_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("client-auth-cert-details-validity-period"),
                value=REPLACE(
                    pippki_properties,
                    "clientAuthValidityPeriod",
                    {
                        "%1$S": VARIABLE_REFERENCE("notBefore"),
                        "%2$S": VARIABLE_REFERENCE("notAfter"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("client-auth-cert-remember-box"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(
                            pippki_properties,
                            "clientAuthRemember",
                        ),
                    )
                ],
            ),
        ],
    )

    for ftl_name, properties_name, variable_name in [
        ("issued-to", "IssuedTo", "issuedTo"),
        ("serial-number", "Serial", "serialNumber"),
        ("key-usages", "KeyUsages", "keyUsages"),
        ("email-addresses", "EmailAddresses", "emailAddresses"),
        ("issued-by", "IssuedBy", "issuedBy"),
        ("stored-on", "StoredOn", "storedOn"),
    ]:
        properties_id = f"clientAuth{properties_name}"
        ctx.add_transforms(
            pippki_ftl,
            pippki_ftl,
            [
                FTL.Message(
                    id=FTL.Identifier(f"client-auth-cert-details-{ftl_name}"),
                    value=REPLACE(
                        pippki_properties,
                        properties_id,
                        {
                            "%1$S": VARIABLE_REFERENCE(variable_name),
                        },
                    ),
                ),
            ],
        )
