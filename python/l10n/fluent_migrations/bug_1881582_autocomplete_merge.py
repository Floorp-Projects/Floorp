# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1881582 - Merge form autofill autocomplete items into normal autocomplete UI part {index}."""

    propertiesSource = "browser/extensions/formautofill/formautofill.properties"
    target = "toolkit/toolkit/formautofill/formAutofill.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("autofill-clear-form-label"),
                value=COPY(propertiesSource, "clearFormBtnLabel2"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-category-address"),
                value=COPY(propertiesSource, "category.address"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-category-name"),
                value=COPY(propertiesSource, "category.name"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-category-organization"),
                value=COPY(propertiesSource, "category.organization2"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-category-tel"),
                value=COPY(propertiesSource, "category.tel"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-category-email"),
                value=COPY(propertiesSource, "category.email"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-phishing-warningmessage-extracategory"),
                value=REPLACE(
                    propertiesSource,
                    "phishingWarningMessage",
                    {
                        "%1$S": VARIABLE_REFERENCE("categories"),
                    },
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-phishing-warningmessage"),
                value=REPLACE(
                    propertiesSource,
                    "phishingWarningMessage2",
                    {
                        "%1$S": VARIABLE_REFERENCE("categories"),
                    },
                ),
            ),
        ],
    )
