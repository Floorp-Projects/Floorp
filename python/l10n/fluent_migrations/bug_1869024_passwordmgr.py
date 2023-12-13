# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.transforms import COPY, REPLACE
from fluent.migrate.helpers import VARIABLE_REFERENCE


def migrate(ctx):
    """Bug 1869024 - Convert passwordmgr.properties to Fluent, part {index}."""

    source = "toolkit/chrome/passwordmgr/passwordmgr.properties"
    target = "toolkit/toolkit/passwordmgr/passwordmgr.ftl"
    ctx.add_transforms(
        target,
        target,
        [
            FTL.Message(
                id=FTL.Identifier("password-manager-save-password-button-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "saveLoginButtonAllow.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "saveLoginButtonAllow.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-save-password-button-never"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "saveLoginButtonNever.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "saveLoginButtonNever.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-update-login-add-username"),
                value=COPY(source, "updateLoginMsgAddUsername2"),
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-password-password-button-allow"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "updateLoginButtonText"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "updateLoginButtonAccessKey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-update-password-button-deny"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "updateLoginButtonDeny.label"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "updateLoginButtonDeny.accesskey"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-no-username-placeholder"),
                value=COPY(source, "noUsernamePlaceholder"),
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-toggle-password"),
                attributes=[
                    FTL.Attribute(
                        id=FTL.Identifier("label"),
                        value=COPY(source, "togglePasswordLabel"),
                    ),
                    FTL.Attribute(
                        id=FTL.Identifier("accesskey"),
                        value=COPY(source, "togglePasswordAccessKey2"),
                    ),
                ],
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-confirm-password-change"),
                value=COPY(source, "passwordChangeTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-select-username"),
                value=COPY(source, "userSelectText2"),
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-save-password-message"),
                value=REPLACE(
                    source,
                    "saveLoginMsgNoUser2",
                    {
                        "%1$S": VARIABLE_REFERENCE("host"),
                    },
                    normalize_printf=True,
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("password-manager-update-password-message"),
                value=REPLACE(
                    source,
                    "updateLoginMsgNoUser3",
                    {
                        "%1$S": VARIABLE_REFERENCE("host"),
                    },
                    normalize_printf=True,
                ),
            ),
        ],
    )
