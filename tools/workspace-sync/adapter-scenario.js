// SPDX-License-Identifier: MPL-2.0
// Manual browser scenario; only the isolated-profile driver invokes this.
// deno-lint-ignore no-unused-vars
async function runScenario() {
  if (
    !Services.prefs.getBoolPref("floorp.workspaces.sync.manual-test", false)
  ) throw new Error("Dedicated test profile required");
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  const { WorkspacesSyncController, createRoot, testingStore } = ChromeUtils
    .importESModule(
      "resource://floorp-workspace-sync-validation/adapter.sys.mjs",
    );
  const { FloorpWorkspacesSync: api } = ChromeUtils.importESModule(
    "resource://floorp-workspace-sync-validation/engine.sys.mjs",
  );
  const { SessionStore } = ChromeUtils.importESModule(
    "moz-src:///browser/components/sessionstore/SessionStore.sys.mjs",
  );
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  await api.ready();
  Services.obs.notifyObservers(null, "weave:service:start-over");
  await new Promise((resolve) => win.setTimeout(resolve, 20));
  const uuid = () =>
    Services.uuid.generateUUID().toString().replace(/[{}]/g, "");
  const assert = (x, m) => {
    if (!x) throw new Error(m);
  };
  const workspace = win.workspacesFuncs.createNoNameWorkspace(),
    otherWorkspace = win.workspacesFuncs.createNoNameWorkspace(),
    tabA = uuid(),
    tabB = uuid(),
    device = uuid();
  const identity = ContextualIdentityService.create(
    "Workspace Sync Test",
    "fingerprint",
    "blue",
  );
  const metadata = {
    name: "Adapter test",
    icon: null,
    userContextId: identity.userContextId,
    isSelected: null,
    isDefault: null,
  };
  const actualStore = JSON.parse(
    Services.prefs.getStringPref("floorp.workspaces.v4.store"),
  );
  actualStore.data = actualStore.data.map((
    [id, value],
  ) => [
    id,
    id === workspace || id === otherWorkspace
      ? { ...value, userContextId: identity.userContextId }
      : value,
  ]);
  Services.prefs.setStringPref(
    "floorp.workspaces.v4.store",
    JSON.stringify(actualStore),
  );
  await new Promise((resolve) => win.setTimeout(resolve, 20));
  testingStore.data.set(workspace, metadata);
  testingStore.data.set(otherWorkspace, { ...metadata, name: "Other" });
  const ctx = {
    getRawWorkspace: (id) => testingStore.data.get(id),
    withExplicitTabUserContext: (_id, open) => open(),
    tabManagerCtx: {
      setWorkspaceIdToAttribute: (tab, id) =>
        tab.setAttribute("floorpWorkspaceId", id),
      updateTabsVisibility: () => {},
    },
  };
  let dispose, constructionError;
  const controller = createRoot((d) => {
    dispose = d;
    try {
      return new WorkspacesSyncController(ctx);
    } catch (error) {
      constructionError = String(error) + " " + error.stack;
      return null;
    }
  });
  if (!controller) throw new Error(constructionError);
  const originalTabs = new Set(win.gBrowser.tabs);
  try {
    for (let i = 0; i < 100 && !controller.isAvailable(); i++) {
      await new Promise((r) => win.setTimeout(r, 20));
    }
    assert(
      controller.isAvailable(),
      "controller registers after SessionStore restoration",
    );
    const document = {
      version: 1,
      workspaceId: workspace,
      name: "Adapter test",
      icon: null,
      metadata: { modifiedAt: 100, modifiedBy: device },
      tabs: [tabA, tabB].map((id, position) => ({
        id,
        url: "__WORKSPACE_SYNC_TEST_ORIGIN__/" + id,
        title: id,
        pinned: false,
        position,
        deleted: false,
        modifiedAt: 100,
        modifiedBy: device,
      })),
    };
    await controller.apply(document, new Set([tabA, tabB]));
    const tabs = [...win.gBrowser.tabs].filter((tab) =>
      tab.getAttribute("floorpWorkspaceId") === workspace &&
      [tabA, tabB].includes(
        SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id"),
      )
    );
    assert(tabs.length === 2, "remote tabs created once");
    assert(
      tabs.every((tab) =>
        Number(tab.getAttribute("usercontextid")) === identity.userContextId
      ),
      "remote tabs use receiver's local workspace container",
    );
    assert(
      new Set(
        tabs.map((tab) =>
          SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id")
        ),
      ).size === 2,
      "stable tab IDs persisted through SessionStore",
    );
    let snapshot = controller.capture(workspace, new Set());
    assert(
      snapshot.tabs.length === 2 && !snapshot.orderChanged,
      "remote apply establishes physical-order baseline",
    );
    const positions = [...win.gBrowser.tabs];
    win.gBrowser.moveTabTo(tabs[1], { tabIndex: positions.indexOf(tabs[0]) });
    snapshot = controller.capture(workspace, new Set());
    assert(snapshot.orderChanged, "actual local tab reorder is detected");
    await controller.apply(document, new Set());
    assert(
      !controller.capture(workspace, new Set()).orderChanged,
      "receiving remote order does not echo a reorder",
    );
    const duplicate = win.gBrowser.addTab(
      "__WORKSPACE_SYNC_TEST_ORIGIN__/duplicate",
      {
        userContextId: identity.userContextId,
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
      },
    );
    duplicate.setAttribute("floorpWorkspaceId", otherWorkspace);
    SessionStore.setCustomTabValue(duplicate, "floorp-workspace-sync-id", tabA);
    SessionStore.setCustomTabValue(
      duplicate,
      "floorp-workspace-sync-owner",
      workspace,
    );
    SessionStore.setCustomTabValue(
      duplicate,
      "floorp-workspace-sync-url",
      "__WORKSPACE_SYNC_TEST_ORIGIN__/duplicate",
    );
    const moved = controller.capture(otherWorkspace, new Set());
    assert(
      moved.tabs.length === 1 && moved.tabs[0].id !== tabA,
      "moving or duplicating a tab to another workspace gets a new global ID",
    );
    await new Promise((resolve) => win.setTimeout(resolve, 500));
    await api.select(workspace, true);
    const closeTarget = [...win.gBrowser.tabs].find((tab) =>
      SessionStore.getCustomTabValue(tab, "floorp-workspace-sync-id") === tabA
    );
    win.gBrowser.removeTab(closeTarget, { animate: false });
    await new Promise((resolve) => win.setTimeout(resolve, 200));
    await api.capture();
    assert(
      api.isRetired(tabA),
      "local tab close retires its UUID " + JSON.stringify({
        selected: api.isSelected(workspace),
        error: api.errorFor(workspace),
        tabs: controller.capture(workspace, new Set()),
        closed: !closeTarget.isConnected,
        custom: SessionStore.getCustomTabValue(
          closeTarget,
          "floorp-workspace-sync-id",
        ),
      }),
    );
    await new Promise((resolve) => win.setTimeout(resolve, 200));
    const reopened = SessionStore.undoCloseTab(win, 0);
    await new Promise((resolve) => win.setTimeout(resolve, 400));
    const restoredSnapshot = controller.capture(
      reopened.getAttribute("floorpWorkspaceId"),
      new Set(),
    );
    const restoredID = SessionStore.getCustomTabValue(
      reopened,
      "floorp-workspace-sync-id",
    );
    assert(
      restoredID !== tabA &&
        restoredSnapshot.tabs.some((tab) => tab.id === restoredID),
      "native Undo Close assigns a new UUID after extData restoration " +
        JSON.stringify({
          restoredID,
          tabA,
          restoredSnapshot,
          owner: reopened.getAttribute("floorpWorkspaceId"),
          explicit: controller.explicitlyOpened.has(reopened),
          after: controller.openedAfterStartup?.has(reopened),
        }),
    );
    await api.select(workspace, false);
    const menu = win.document.createXULElement("menuitem");
    menu.setAttribute("type", "checkbox");
    menu.setAttribute("autocheck", "false");
    menu.dispatchEvent(new win.Event("command", { bubbles: true }));
    assert(
      !menu.hasAttribute("checked"),
      "checkbox does not toggle ahead of asynchronous selection",
    );
    return {
      passed: true,
      cases: [
        "native tab creation and local container",
        "SessionStore stable IDs",
        "local reorder detection and receive baseline",
        "global ID regeneration on workspace move",
        "checkbox state ownership",
        "native Undo Close after SessionStore restoration",
      ],
    };
  } finally {
    try {
      dispose();
    } catch (error) {
      console.error(error);
    }
    for (const tab of [...win.gBrowser.tabs]) {
      if (!originalTabs.has(tab)) {
        win.gBrowser.removeTab(tab, { animate: false });
      }
    }
    ContextualIdentityService.remove(identity.userContextId);
  }
}
