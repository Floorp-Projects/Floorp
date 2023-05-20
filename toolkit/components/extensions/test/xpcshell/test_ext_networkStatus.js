"use strict";

const Cm = Components.manager;

const uuidGenerator = Services.uuid;

AddonTestUtils.init(this);

var mockNetworkStatusService = {
  contractId: "@mozilla.org/network/network-link-service;1",

  _mockClassId: uuidGenerator.generateUUID(),

  _originalClassId: "",

  QueryInterface: ChromeUtils.generateQI(["nsINetworkLinkService"]),

  createInstance(iiD) {
    return this.QueryInterface(iiD);
  },

  register() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    if (!registrar.isCIDRegistered(this._mockClassId)) {
      this._originalClassId = registrar.contractIDToCID(this.contractId);
      registrar.registerFactory(
        this._mockClassId,
        "Unregister after testing",
        this.contractId,
        this
      );
    }
  },

  unregister() {
    let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(this._mockClassId, this);
    registrar.registerFactory(this._originalClassId, "", this.contractId, null);
  },

  _isLinkUp: true,
  _linkStatusKnown: false,
  _linkType: Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN,

  get isLinkUp() {
    return this._isLinkUp;
  },

  get linkStatusKnown() {
    return this._linkStatusKnown;
  },

  setLinkStatus(status) {
    switch (status) {
      case "up":
        this._isLinkUp = true;
        this._linkStatusKnown = true;
        this._networkID = "foo";
        break;
      case "down":
        this._isLinkUp = false;
        this._linkStatusKnown = true;
        this._linkType = Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN;
        this._networkID = undefined;
        break;
      case "changed":
        this._linkStatusKnown = true;
        this._networkID = "foo";
        break;
      case "unknown":
        this._linkStatusKnown = false;
        this._linkType = Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN;
        this._networkID = undefined;
        break;
    }
    Services.obs.notifyObservers(null, "network:link-status-changed", status);
  },

  get linkType() {
    return this._linkType;
  },

  setLinkType(val) {
    this._linkType = val;
    this._linkStatusKnown = true;
    this._isLinkUp = true;
    this._networkID = "bar";
    Services.obs.notifyObservers(
      null,
      "network:link-type-changed",
      this._linkType
    );
  },

  get networkID() {
    return this._networkID;
  },
};

// nsINetworkLinkService is not directly testable. With the mock service above,
// we just exercise a couple small things here to validate the api works somewhat.
add_task(async function test_networkStatus() {
  mockNetworkStatusService.register();
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: { id: "networkstatus@tests.mozilla.org" },
      },
      permissions: ["networkStatus"],
    },
    isPrivileged: true,
    async background() {
      browser.networkStatus.onConnectionChanged.addListener(async details => {
        browser.test.log(`connection status ${JSON.stringify(details)}`);
        browser.test.sendMessage("connect-changed", {
          details,
          linkInfo: await browser.networkStatus.getLinkInfo(),
        });
      });
      browser.test.sendMessage(
        "linkdata",
        await browser.networkStatus.getLinkInfo()
      );
    },
  });

  async function test(expected, change) {
    if (change.status) {
      info(`test link change status to ${change.status}`);
      mockNetworkStatusService.setLinkStatus(change.status);
    } else if (change.link) {
      info(`test link change type to ${change.link}`);
      mockNetworkStatusService.setLinkType(change.link);
    }
    let { details, linkInfo } = await extension.awaitMessage("connect-changed");
    equal(details.type, expected.type, "network type is correct");
    equal(details.status, expected.status, `network status is correct`);
    equal(details.id, expected.id, "network id");
    Assert.deepEqual(
      linkInfo,
      details,
      "getLinkInfo should resolve to the same details received from onConnectionChanged"
    );
  }

  await extension.startup();

  let data = await extension.awaitMessage("linkdata");
  equal(data.type, "unknown", "network type is unknown");
  equal(data.status, "unknown", `network status is ${data.status}`);
  equal(data.id, undefined, "network id");

  await test(
    { type: "unknown", status: "up", id: "foo" },
    { status: "changed" }
  );

  await test(
    { type: "wifi", status: "up", id: "bar" },
    { link: Ci.nsINetworkLinkService.LINK_TYPE_WIFI }
  );

  await test({ type: "unknown", status: "down" }, { status: "down" });

  await test({ type: "unknown", status: "unknown" }, { status: "unknown" });

  await extension.unload();
  mockNetworkStatusService.unregister();
});

add_task(
  {
    // Some builds (e.g. thunderbird) have experiments enabled by default.
    pref_set: [["extensions.experiments.enabled", false]],
  },
  async function test_networkStatus_permission() {
    let extension = ExtensionTestUtils.loadExtension({
      temporarilyInstalled: true,
      isPrivileged: false,
      manifest: {
        browser_specific_settings: {
          gecko: { id: "networkstatus-permission@tests.mozilla.org" },
        },
        permissions: ["networkStatus"],
      },
    });
    ExtensionTestUtils.failOnSchemaWarnings(false);
    let { messages } = await promiseConsoleOutput(async () => {
      await Assert.rejects(
        extension.startup(),
        /Using the privileged permission/,
        "Startup failed with privileged permission"
      );
    });
    ExtensionTestUtils.failOnSchemaWarnings(true);
    AddonTestUtils.checkMessages(
      messages,
      {
        expected: [
          {
            message:
              /Using the privileged permission 'networkStatus' requires a privileged add-on/,
          },
        ],
      },
      true
    );
  }
);
