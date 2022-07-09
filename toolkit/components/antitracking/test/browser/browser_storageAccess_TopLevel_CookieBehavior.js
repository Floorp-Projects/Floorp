add_task(async function testBehaviorAcceptRequestStorageAccessUnderSite() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_ACCEPT],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_3RD_PARTY_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.requestStorageAccessUnderSite(tp);
    try {
      await p;
      ok(true, "Must resolve.");
    } catch {
      ok(false, "Must not reject.");
    }
  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testBehaviorAcceptCompleteStorageAccessRequest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_ACCEPT],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
    await SpecialPowers.pushPermissions([
      {
        type: "AllowStorageAccessRequest^http://example.org",
        allow: 1,
        context: content.document,
      },
    ]);
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(true, "Must resolve.");
    } catch {
      ok(false, "Must not reject.");
    }
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testBehaviorRejectRequestStorageAccessUnderSite() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_REJECT],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_3RD_PARTY_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.requestStorageAccessUnderSite(tp);
    try {
      await p;
      ok(false, "Must not resolve.");
    } catch {
      ok(true, "Must reject.");
    }
  });

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testBehaviorRejectCompleteStorageAccessRequest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_REJECT],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
    await SpecialPowers.pushPermissions([
      {
        type: "AllowStorageAccessRequest^http://example.org",
        allow: 1,
        context: content.document,
      },
    ]);
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(false, "Must not resolve.");
    } catch {
      ok(true, "Must reject.");
    }
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(
  async function testBehaviorLimitForeignRequestStorageAccessUnderSite() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["dom.storage_access.forward_declared.enabled", true],
        ["network.cookie.cookieBehavior", BEHAVIOR_LIMIT_FOREIGN],
        ["dom.storage_access.auto_grants", false],
        ["dom.storage_access.max_concurrent_auto_grants", 1],
      ],
    });
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_3RD_PARTY_PAGE,
    });
    let browser = tab.linkedBrowser;
    await SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      var p = content.document.requestStorageAccessUnderSite(tp);
      try {
        await p;
        ok(false, "Must not resolve.");
      } catch {
        ok(true, "Must reject.");
      }
    });

    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(async function testBehaviorLimitForeignCompleteStorageAccessRequest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["network.cookie.cookieBehavior", BEHAVIOR_LIMIT_FOREIGN],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
    await SpecialPowers.pushPermissions([
      {
        type: "AllowStorageAccessRequest^http://example.org",
        allow: 1,
        context: content.document,
      },
    ]);
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    var p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(false, "Must not resolve.");
    } catch {
      ok(true, "Must reject.");
    }
  });
  await BrowserTestUtils.removeTab(tab);
});

add_task(
  async function testBehaviorRejectForeignRequestStorageAccessUnderSite() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["dom.storage_access.forward_declared.enabled", true],
        ["network.cookie.cookieBehavior", BEHAVIOR_REJECT_FOREIGN],
        ["dom.storage_access.auto_grants", false],
        ["dom.storage_access.max_concurrent_auto_grants", 1],
      ],
    });
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_3RD_PARTY_PAGE,
    });
    let browser = tab.linkedBrowser;
    await SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      var p = content.document.requestStorageAccessUnderSite(tp);
      try {
        await p;
        ok(false, "Must not resolve.");
      } catch {
        ok(true, "Must reject.");
      }
    });

    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(
  async function testBehaviorRejectForeignCompleteStorageAccessRequest() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["dom.storage_access.forward_declared.enabled", true],
        ["network.cookie.cookieBehavior", BEHAVIOR_REJECT_FOREIGN],
        ["dom.storage_access.auto_grants", false],
        ["dom.storage_access.max_concurrent_auto_grants", 1],
      ],
    });
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_TOP_PAGE,
    });
    let browser = tab.linkedBrowser;
    await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
      await SpecialPowers.pushPermissions([
        {
          type: "AllowStorageAccessRequest^http://example.org",
          allow: 1,
          context: content.document,
        },
      ]);
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      var p = content.document.completeStorageAccessRequestFromSite(tp);
      try {
        await p;
        ok(false, "Must not resolve.");
      } catch {
        ok(true, "Must reject.");
      }
    });
    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(
  async function testBehaviorRejectTrackerRequestStorageAccessUnderSite() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["dom.storage_access.forward_declared.enabled", true],
        ["network.cookie.cookieBehavior", BEHAVIOR_REJECT_TRACKER],
        ["dom.storage_access.auto_grants", false],
        ["dom.storage_access.max_concurrent_auto_grants", 1],
      ],
    });
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_3RD_PARTY_DOMAIN,
    });
    let browser = tab.linkedBrowser;
    await SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      var p = content.document.requestStorageAccessUnderSite(tp);
      try {
        await p;
        ok(true, "Must resolve.");
      } catch {
        ok(false, "Must not reject.");
      }
    });

    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(
  async function testBehaviorRejectTrackerCompleteStorageAccessRequest() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["dom.storage_access.forward_declared.enabled", true],
        ["network.cookie.cookieBehavior", BEHAVIOR_REJECT_TRACKER],
        ["dom.storage_access.auto_grants", false],
        ["dom.storage_access.max_concurrent_auto_grants", 1],
      ],
    });
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_TOP_PAGE,
    });
    let browser = tab.linkedBrowser;
    await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
      await SpecialPowers.pushPermissions([
        {
          type: "AllowStorageAccessRequest^http://example.com",
          allow: 1,
          context: content.document,
        },
      ]);
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      var p = content.document.completeStorageAccessRequestFromSite(tp);
      try {
        await p;
        ok(true, "Must resolve.");
      } catch {
        ok(false, "Must not reject.");
      }
    });
    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(
  async function testBehaviorRejectTrackerAndPartitionForeignRequestStorageAccessUnderSite() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["dom.storage_access.forward_declared.enabled", true],
        [
          "network.cookie.cookieBehavior",
          BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
        ],
        ["dom.storage_access.auto_grants", false],
        ["dom.storage_access.max_concurrent_auto_grants", 1],
      ],
    });
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_3RD_PARTY_DOMAIN,
    });
    let browser = tab.linkedBrowser;
    await SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      var p = content.document.requestStorageAccessUnderSite(tp);
      try {
        await p;
        ok(true, "Must resolve.");
      } catch {
        ok(false, "Must not reject.");
      }
    });

    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(
  async function testBehaviorRejectTrackerAndPartitionForeignCompleteStorageAccessRequest() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["dom.storage_access.forward_declared.enabled", true],
        [
          "network.cookie.cookieBehavior",
          BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
        ],
        ["dom.storage_access.auto_grants", false],
        ["dom.storage_access.max_concurrent_auto_grants", 1],
      ],
    });
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_TOP_PAGE,
    });
    let browser = tab.linkedBrowser;
    await SpecialPowers.spawn(browser, [TEST_3RD_PARTY_DOMAIN], async tp => {
      await SpecialPowers.pushPermissions([
        {
          type: "AllowStorageAccessRequest^http://example.com",
          allow: 1,
          context: content.document,
        },
      ]);
      SpecialPowers.wrap(content.document).notifyUserGestureActivation();
      var p = content.document.completeStorageAccessRequestFromSite(tp);
      try {
        await p;
        ok(true, "Must resolve.");
      } catch {
        ok(false, "Must not reject.");
      }
    });
    await BrowserTestUtils.removeTab(tab);
  }
);

add_task(async () => {
  Services.perms.removeAll();
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
