# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import TERM_REFERENCE, transforms_from
from fluent.migrate.transforms import COPY, REPLACE, Transform


def migrate(ctx):
    """Bugs 1446164 & 1786708 - Migrate formautofill dialogs to Fluent, part {index}."""

    source = "browser/extensions/formautofill/formautofill.properties"
    target = "browser/browser/preferences/formAutofill.ftl"

    ctx.add_transforms(
        target,
        target,
        # Bug 1786708
        transforms_from(
            """
autofill-manage-addresses-title = { COPY(source, "manageAddressesTitle") }
autofill-manage-credit-cards-title = { COPY(source, "manageCreditCardsTitle") }
autofill-manage-addresses-list-header = { COPY(source, "addressesListHeader") }
autofill-manage-credit-cards-list-header = { COPY(source, "creditCardsListHeader") }
autofill-manage-remove-button = { COPY(source, "removeBtnLabel") }
autofill-manage-add-button = { COPY(source, "addBtnLabel") }
autofill-manage-edit-button = { COPY(source, "editBtnLabel") }
autofill-manage-dialog =
    .style = min-width: { COPY(source, "manageDialogsWidth") }
            """,
            source=source,
        )
        + [
            FTL.Message(
                FTL.Identifier("autofill-edit-card-password-prompt"),
                value=Transform.pattern_of(
                    FTL.SelectExpression(
                        selector=FTL.FunctionReference(
                            id=FTL.Identifier("PLATFORM"), arguments=FTL.CallArguments()
                        ),
                        variants=[
                            FTL.Variant(
                                key=FTL.Identifier("macos"),
                                value=COPY(
                                    source, "editCreditCardPasswordPrompt.macos"
                                ),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("windows"),
                                value=REPLACE(
                                    source,
                                    "editCreditCardPasswordPrompt.win",
                                    {
                                        "%1$S": TERM_REFERENCE("brand-short-name"),
                                    },
                                ),
                            ),
                            FTL.Variant(
                                key=FTL.Identifier("other"),
                                value=REPLACE(
                                    source,
                                    "editCreditCardPasswordPrompt.linux",
                                    {
                                        "%1$S": TERM_REFERENCE("brand-short-name"),
                                    },
                                ),
                                default=True,
                            ),
                        ],
                    )
                ),
            )
        ]
        +
        # Bug 1446164
        [
            FTL.Message(
                id=FTL.Identifier("autofill-add-new-address-title"),
                value=COPY(source, "addNewAddressTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-edit-address-title"),
                value=COPY(source, "editAddressTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-given-name"),
                value=COPY(source, "givenName"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-additional-name"),
                value=COPY(source, "additionalName"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-family-name"),
                value=COPY(source, "familyName"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-organization"),
                value=COPY(source, "organization2"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-street"),
                value=COPY(source, "streetAddress"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-neighborhood"),
                value=COPY(source, "neighborhood"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-village-township"),
                value=COPY(source, "village_township"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-island"),
                value=COPY(source, "island"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-townland"),
                value=COPY(source, "townland"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-city"), value=COPY(source, "city")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-district"),
                value=COPY(source, "district"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-post-town"),
                value=COPY(source, "post_town"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-suburb"),
                value=COPY(source, "suburb"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-province"),
                value=COPY(source, "province"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-state"), value=COPY(source, "state")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-county"),
                value=COPY(source, "county"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-parish"),
                value=COPY(source, "parish"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-prefecture"),
                value=COPY(source, "prefecture"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-area"), value=COPY(source, "area")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-do-si"), value=COPY(source, "do_si")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-department"),
                value=COPY(source, "department"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-emirate"),
                value=COPY(source, "emirate"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-oblast"),
                value=COPY(source, "oblast"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-pin"), value=COPY(source, "pin")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-postal-code"),
                value=COPY(source, "postalCode"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-zip"), value=COPY(source, "zip")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-eircode"),
                value=COPY(source, "eircode"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-country"),
                value=COPY(source, "country"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-tel"), value=COPY(source, "tel")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-address-email"), value=COPY(source, "email")
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-cancel-button"),
                value=COPY(source, "cancelBtnLabel"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-save-button"),
                value=COPY(source, "saveBtnLabel"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-country-warning-message"),
                value=COPY(source, "countryWarningMessage2"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-add-new-card-title"),
                value=COPY(source, "addNewCreditCardTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-edit-card-title"),
                value=COPY(source, "editCreditCardTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-number"),
                value=COPY(source, "cardNumber"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-invalid-number"),
                value=COPY(source, "invalidCardNumber"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-name-on-card"),
                value=COPY(source, "nameOnCard"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-expires-month"),
                value=COPY(source, "cardExpiresMonth"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-expires-year"),
                value=COPY(source, "cardExpiresYear"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-billing-address"),
                value=COPY(source, "billingAddress"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network"),
                value=COPY(source, "cardNetwork"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-amex"),
                value=COPY(source, "cardNetwork.amex"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-cartebancaire"),
                value=COPY(source, "cardNetwork.cartebancaire"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-diners"),
                value=COPY(source, "cardNetwork.diners"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-discover"),
                value=COPY(source, "cardNetwork.discover"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-jcb"),
                value=COPY(source, "cardNetwork.jcb"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-mastercard"),
                value=COPY(source, "cardNetwork.mastercard"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-mir"),
                value=COPY(source, "cardNetwork.mir"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-unionpay"),
                value=COPY(source, "cardNetwork.unionpay"),
            ),
            FTL.Message(
                id=FTL.Identifier("autofill-card-network-visa"),
                value=COPY(source, "cardNetwork.visa"),
            ),
        ],
    )
