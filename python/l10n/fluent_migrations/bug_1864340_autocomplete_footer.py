# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY, COPY_PATTERN


def migrate(ctx):
    """Bug 1864340 - Convert autocomplete footer strings to FTL, part {index}."""

    propertiesSource = "browser/extensions/formautofill/formautofill.properties"
    fluentSource = "browser/browser/preferences/formAutofill.ftl"
    target = "toolkit/toolkit/formautofill/formAutofill.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("autofill-manage-addresses-label"),
                value=COPY(propertiesSource, "autocompleteManageAddresses"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-amex"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-amex"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-cartebancaire"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-cartebancaire"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-diners"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-diners"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-discover"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-discover"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-jcb"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-jcb"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-mastercard"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-mastercard"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-mir"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-mir"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-unionpay"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-unionpay"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-visa"),
                value=COPY_PATTERN(fluentSource, "autofill-card-network-visa"),
            ),
        ],
    )
