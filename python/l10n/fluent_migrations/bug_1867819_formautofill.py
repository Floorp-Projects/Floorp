# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY


def migrate(ctx):
    """Bug 1867819 - Convert formautofill.properties to Fluent, part {index}."""

    source = "browser/extensions/formautofill/formautofill.properties"
    target = "toolkit/toolkit/formautofill/formAutofill.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("autofill-options-link"),
                value=COPY(source, "autofillOptionsLink"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-options-link-osx"),
                value=COPY(source, "autofillOptionsLinkOSX"),
            ),
        ],
    )
