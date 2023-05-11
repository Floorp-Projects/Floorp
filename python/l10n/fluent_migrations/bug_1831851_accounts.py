# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, PLURALS, REPLACE, REPLACE_IN_TEXT


def migrate(ctx):
    """Bug 1831851 - Migrate accounts.properties to Fluent, part {index}."""

    accounts = "browser/chrome/browser/accounts.properties"
    accounts_ftl = "browser/browser/accounts.ftl"
    preferences_ftl = "browser/browser/preferences/preferences.ftl"

    ctx.add_transforms(
        accounts_ftl,
        accounts_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("account-reconnect"),
                value=REPLACE(
                    accounts,
                    "reconnectDescription",
                    {"%1$S": VARIABLE_REFERENCE("email")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("account-verify"),
                value=REPLACE(
                    accounts, "verifyDescription", {"%1$S": VARIABLE_REFERENCE("email")}
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("account-send-to-all-devices-titlecase"),
                value=COPY(accounts, "sendToAllDevices.menuitem"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-manage-devices-titlecase"),
                value=COPY(accounts, "manageDevices.menuitem"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-send-tab-to-device-singledevice-status"),
                value=COPY(accounts, "sendTabToDevice.singledevice.status"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-send-tab-to-device-singledevice-learnmore"),
                value=COPY(accounts, "sendTabToDevice.singledevice"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-send-tab-to-device-connectdevice"),
                value=COPY(accounts, "sendTabToDevice.connectdevice"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-send-tab-to-device-verify-status"),
                value=COPY(accounts, "sendTabToDevice.verify.status"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-send-tab-to-device-verify"),
                value=COPY(accounts, "sendTabToDevice.verify"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-connection-title"),
                value=FTL.Pattern(
                    [
                        FTL.Placeable(
                            FTL.TermReference(
                                id=FTL.Identifier("fxaccount-brand-name"),
                                arguments=FTL.CallArguments(
                                    named=[
                                        FTL.NamedArgument(
                                            FTL.Identifier("capitalization"),
                                            FTL.StringLiteral("title"),
                                        )
                                    ]
                                ),
                            )
                        )
                    ]
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("account-connection-connected-with"),
                value=REPLACE(
                    accounts,
                    "otherDeviceConnectedBody",
                    {"%1$S": VARIABLE_REFERENCE("deviceName")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("account-connection-connected-with-noname"),
                value=COPY(accounts, "otherDeviceConnectedBody.noDeviceName"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-connection-connected"),
                value=COPY(accounts, "thisDeviceConnectedBody"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-connection-disconnected"),
                value=COPY(accounts, "thisDeviceDisconnectedBody"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-single-tab-arriving-title"),
                value=COPY(accounts, "tabArrivingNotification.title"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-single-tab-arriving-from-device-title"),
                value=REPLACE(
                    accounts,
                    "tabArrivingNotificationWithDevice.title",
                    {"%1$S": VARIABLE_REFERENCE("deviceName")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("account-single-tab-arriving-truncated-url"),
                value=REPLACE(
                    accounts,
                    "singleTabArrivingWithTruncatedURL.body",
                    {"%1$S": VARIABLE_REFERENCE("url")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("account-multiple-tabs-arriving-title"),
                value=COPY(accounts, "multipleTabsArrivingNotification.title"),
            ),
            FTL.Message(
                id=FTL.Identifier("account-multiple-tabs-arriving-from-single-device"),
                value=PLURALS(
                    accounts,
                    "unnamedTabsArrivingNotification2.body",
                    VARIABLE_REFERENCE("tabCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {
                            "#1": VARIABLE_REFERENCE("tabCount"),
                            "#2": VARIABLE_REFERENCE("deviceName"),
                        },
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier(
                    "account-multiple-tabs-arriving-from-multiple-devices"
                ),
                value=PLURALS(
                    accounts,
                    "unnamedTabsArrivingNotificationMultiple2.body",
                    VARIABLE_REFERENCE("tabCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("tabCount")},
                    ),
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("account-multiple-tabs-arriving-from-unknown-device"),
                value=PLURALS(
                    accounts,
                    "unnamedTabsArrivingNotificationNoDevice.body",
                    VARIABLE_REFERENCE("tabCount"),
                    foreach=lambda n: REPLACE_IN_TEXT(
                        n,
                        {"#1": VARIABLE_REFERENCE("tabCount")},
                    ),
                ),
            ),
        ],
    )

    ctx.add_transforms(
        preferences_ftl,
        preferences_ftl,
        [
            FTL.Message(
                id=FTL.Identifier("sync-verification-sent-title"),
                value=COPY(accounts, "verificationSentTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("sync-verification-sent-body"),
                value=REPLACE(
                    accounts,
                    "verificationSentBody",
                    {"%1$S": VARIABLE_REFERENCE("email")},
                ),
            ),
            FTL.Message(
                id=FTL.Identifier("sync-verification-not-sent-title"),
                value=COPY(accounts, "verificationNotSentTitle"),
            ),
            FTL.Message(
                id=FTL.Identifier("sync-verification-not-sent-body"),
                value=COPY(accounts, "verificationNotSentBody"),
            ),
        ],
    )
