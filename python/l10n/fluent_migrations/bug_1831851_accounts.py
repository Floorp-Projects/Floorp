# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import fluent.syntax.ast as FTL
from fluent.migrate.helpers import VARIABLE_REFERENCE
from fluent.migrate.transforms import COPY, REPLACE


def migrate(ctx):
    """Bug 1831851 - Migrate accounts.properties to Fluent, part {index}."""

    accounts = "browser/chrome/browser/accounts.properties"
    accounts_ftl = "browser/browser/accounts.ftl"
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
        ],
    )
