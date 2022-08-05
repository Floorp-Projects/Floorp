/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Return card containers matching a given name
function getCardsByName(aContainer, aLeafName) {
  const matchedCards = [];
  const allCards = aContainer.querySelectorAll(".card");
  for (const card of allCards) {
    const nameLabel = card.querySelector(".module-name");
    if (nameLabel.textContent == aLeafName) {
      matchedCards.push(card);
    }
  }
  return matchedCards;
}

function getDetailRow(aContainer, aLabel) {
  return aContainer.querySelector(`[data-l10n-id=${aLabel}]`).parentElement;
}

function verifyClipboardData(aModuleArray) {
  const filtered = aModuleArray.filter(x => x.name == kExtensionModuleName);
  Assert.equal(filtered.length, 1, "No duplicate data for the module.");

  const kDeletedPropertiesOfModule = [
    "application",
    "dllFile",
    "loadingOnMain",
    "trustFlags",
  ];
  const kDeletedPropertiesOfLoadingEvent = [
    "baseAddress",
    "isDependent",
    "mainThread",
    "moduleIndex",
    "processUptimeMS",
  ];

  for (const module of aModuleArray) {
    for (const deletedProperty of kDeletedPropertiesOfModule) {
      Assert.ok(
        !module.hasOwnProperty(deletedProperty),
        `The property \`${deletedProperty}\` is deleted.`
      );
    }

    Assert.ok(
      !module.hasOwnProperty("typeFlags") || module.typeFlags != 0,
      "typeFlags does not exist or is non-zero."
    );

    for (const event of module.events) {
      for (const deletedProperty of kDeletedPropertiesOfLoadingEvent) {
        Assert.ok(
          !event.hasOwnProperty(deletedProperty),
          `The property \`${deletedProperty}\` is deleted.`
        );
      }
    }
  }
}

add_task(async () => {
  registerCleanupFunction(() => {
    unregisterAll();
  });
  await registerObject();
  registerExtensions();
  loadShellExtension();

  await kATP.collectSystemInfo();
  Assert.equal(
    kATP.lookupModuleType(kExtensionModuleName),
    Ci.nsIAboutThirdParty.ModuleType_ShellExtension,
    "lookupModuleType() returns a correct type " +
      "after system info was collected."
  );

  await BrowserTestUtils.withNewTab("about:third-party", async browser => {
    if (!content.fetchDataDone) {
      const mainDiv = content.document.getElementById("main");
      await BrowserTestUtils.waitForMutationCondition(
        mainDiv,
        { childList: true },
        () => mainDiv.childElementCount > 0
      );
      Assert.ok(content.fetchDataDone, "onLoad() is complated.");
    }

    const reload = content.document.getElementById("button-reload");
    if (!reload.hidden) {
      reload.click();
      await BrowserTestUtils.waitForMutationCondition(
        reload,
        { attributes: true, attributeFilter: ["hidden"] },
        () => reload.hidden
      );
    }

    Assert.ok(
      content.document.getElementById("no-data").hidden,
      "The no-data message is hidden."
    );

    const cards = getCardsByName(content.document, kExtensionModuleName);
    Assert.equal(cards.length, 1, "Only one card matching the module exists.");
    const card = cards[0];

    Assert.ok(
      card.querySelector(".image-warning").hidden,
      "No warning sign for the module."
    );
    Assert.equal(
      card.querySelector(".image-unsigned").hidden,
      false,
      "The module is labeled as unsigned."
    );
    Assert.equal(
      card.querySelector(".tag-shellex").hidden,
      false,
      "The module is labeled as a shell extension."
    );
    Assert.equal(
      card.querySelector(".tag-ime").hidden,
      true,
      "The module is not labeled as an IME."
    );

    const versionRow = getDetailRow(card, "third-party-detail-version");
    Assert.equal(
      versionRow.childNodes[1].textContent,
      "1.2.3.4",
      "The version matches a value in TestShellEx.rc."
    );
    const vendorRow = getDetailRow(card, "third-party-detail-vendor");
    Assert.equal(
      vendorRow.childNodes[1].textContent,
      "Mozilla Corporation",
      "The vendor name matches a value in TestShellEx.rc."
    );
    const occurrencesRow = getDetailRow(card, "third-party-detail-occurrences");
    Assert.equal(
      Number(occurrencesRow.childNodes[1].textContent),
      1,
      "The module was loaded once."
    );
    const durationRow = getDetailRow(card, "third-party-detail-duration");
    Assert.ok(
      Number(durationRow.childNodes[1].textContent),
      "The duration row shows a valid number."
    );

    const eventTable = card.querySelector(".event-table");
    const tableCells = eventTable.querySelectorAll("td");
    Assert.equal(
      tableCells.length,
      3,
      "The table has three cells as there is only one event."
    );
    Assert.equal(
      tableCells[0].querySelector(".process-type").getAttribute("data-l10n-id"),
      "process-type-default",
      "The module was loaded into the main process."
    );
    Assert.ok(
      Number(tableCells[0].querySelector(".process-id").textContent),
      "A valid process ID is displayed."
    );
    Assert.equal(
      tableCells[1].querySelector(".event-duration").textContent,
      durationRow.childNodes[1].textContent,
      "The event's duration is the same as the average " +
        "as there is only one event."
    );
    Assert.equal(
      tableCells[1].querySelector(".tag-background").hidden,
      true,
      "The icon handler is loaded in the main thread."
    );
    Assert.equal(
      tableCells[2].getAttribute("data-l10n-id"),
      "third-party-status-loaded",
      "The module was really loaded without being blocked."
    );

    const button = content.document.getElementById("button-copy-to-clipboard");
    button.click();

    // Wait until copying is done and the button becomes clickable.
    await BrowserTestUtils.waitForMutationCondition(
      button,
      { attributes: true },
      () => !button.disabled
    );

    const copiedJSON = JSON.parse(await navigator.clipboard.readText());
    Assert.ok(copiedJSON instanceof Array, "Data is an array.");
    verifyClipboardData(copiedJSON);
  });
});
