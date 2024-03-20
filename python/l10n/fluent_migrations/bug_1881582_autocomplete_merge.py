# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


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
        ],
    )
