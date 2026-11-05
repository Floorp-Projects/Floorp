// live:data
import { i, b, p, h, s, j, setIsPanelSidebarEnabled, o, q, r } from "chrome://noraneko/content/assets/js/data.js";

// floorp-pr2877-current/browser-features/chrome/common/panel-sidebar/test/panelSidebarLayout.test.ts
import { PanelNavigator } from "chrome://noraneko/content/assets/js/index16.js";
import { gFlexOrder } from "chrome://noraneko/content/assets/js/index10.js";

// floorp-pr2877-current/browser-features/chrome/test/utils/test_harness.ts
function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}
function assertEquals(actual, expected, message) {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`
    );
  }
}
function assertApprox(actual, expected, tolerance, message) {
  if (Math.abs(actual - expected) > tolerance) {
    throw new Error(
      `${message} (expected: ~${expected} \xB1 ${tolerance}, actual: ${actual})`
    );
  }
}
async function runTests(moduleName, tests) {
  const failures = [];
  for (const [index, test] of tests.entries()) {
    globalThis.__NORA_TEST_PROGRESS__ = {
      moduleName,
      testName: test.name,
      status: "running",
      index: index + 1,
      total: tests.length,
      startedAtMs: Date.now()
    };
    try {
      await test.fn();
      const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "passed",
        index: index + 1,
        total: tests.length,
        startedAtMs
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
      const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "failed",
        index: index + 1,
        total: tests.length,
        startedAtMs
      };
    }
  }
  globalThis.__NORA_TEST_PROGRESS__ = {
    moduleName,
    testName: "",
    status: "done",
    index: tests.length,
    total: tests.length,
    startedAtMs: Date.now()
  };
  if (failures.length > 0) {
    throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
  }
}

// floorp-pr2877-current/browser-features/chrome/common/panel-sidebar/test/panelSidebarLayout.test.ts
var testPanelId = "sidebar-layout-regression";
var nextFrame = () => new Promise((resolve) => {
  requestAnimationFrame(() => requestAnimationFrame(() => resolve()));
});
function element(id) {
  const result = document.getElementById(id);
  assert(result, `#${id} should exist`);
  return result;
}
async function withPanel(test) {
  const controller = PanelNavigator.gPanelSidebar;
  assert(controller, "panel sidebar should be initialized");
  const config = p();
  const panels = h();
  const selected = s();
  const floating = i();
  try {
    j(false);
    q([...panels, {
      id: testPanelId,
      type: "web",
      url: "about:blank",
      width: 400,
      icon: void 0,
      userContextId: void 0,
      zoomLevel: void 0,
      userAgent: void 0,
      extensionId: void 0
    }]);
    controller.changePanel(testPanelId);
    await nextFrame();
    await test();
  } finally {
    controller.unloadPanel(testPanelId);
    q(panels);
    o(config);
    gFlexOrder.applyFlexOrder(config.position_start);
    if (selected) controller.changePanel(selected);
    j(floating);
  }
}
async function testWebPanelViewport() {
  await withPanel(async () => {
    const browser = element(`sidebar-panel-${testPanelId}`);
    let child;
    for (let attempt = 0; attempt < 200; attempt++) {
      child = browser.browsingContext?.associatedWindow;
      if (child?.document.getElementById("floorp-webpanel-styles")) break;
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
    assert(
      child && child.document.getElementById("floorp-webpanel-styles"),
      "web panel should initialize"
    );
    for (const width of [225, 400, 600]) {
      element("panel-sidebar-box").style.width = `${width}px`;
      await nextFrame();
      assertApprox(
        child.document.documentElement.getBoundingClientRect().width,
        browser.getBoundingClientRect().width,
        1,
        `child viewport should fit its ${width}px panel instead of the taskbar window minimum`
      );
    }
  });
}
async function testCompletedResizePersists() {
  await withPanel(async () => {
    const controller = PanelNavigator.gPanelSidebar;
    for (const atEnd of [false, true]) {
      o((config) => ({ ...config, position_start: atEnd }));
      await nextFrame();
      const box = element("panel-sidebar-box");
      box.style.width = "475px";
      element("panel-sidebar-splitter").dispatchEvent(new Event("command"));
      const savedWidth = Math.round(box.getBoundingClientRect().width);
      assertEquals(
        controller.getPanelData(testPanelId)?.width,
        savedWidth,
        "completed resize should update panel data"
      );
      const saved = JSON.parse(
        Services.prefs.getStringPref("floorp.panelSidebar.data")
      );
      assertEquals(
        saved.data.find((panel) => panel.id === testPanelId)?.width,
        savedWidth,
        "completed width should be persisted in preferences"
      );
      controller.changePanel(testPanelId);
      await nextFrame();
      controller.saveCurrentSidebarWidth();
      assertEquals(
        controller.getPanelData(testPanelId)?.width,
        savedWidth,
        "hidden panel must not save zero width"
      );
      controller.changePanel(testPanelId);
      await nextFrame();
      assertApprox(
        box.getBoundingClientRect().width,
        savedWidth,
        1,
        "reopening should restore width"
      );
      controller.unloadPanel(testPanelId);
      controller.changePanel(testPanelId);
      await nextFrame();
      assertApprox(
        box.getBoundingClientRect().width,
        savedWidth,
        1,
        "recreating a panel should restore width"
      );
    }
  });
}
async function testFeatureToggleWithVerticalTabs() {
  await withPanel(async () => {
    const root = document.documentElement;
    const verticalPref = "sidebar.verticalTabs";
    const hadVerticalPref = Services.prefs.prefHasUserValue(verticalPref);
    const savedVerticalPref = Services.prefs.getBoolPref(verticalPref, false);
    const enabled = b();
    try {
      Services.prefs.setBoolPref(verticalPref, true);
      await nextFrame();
      assert(
        root.hasAttribute("sidebar-mode") || document.getElementById("sidebar-container") !== null,
        "Firefox sidebar should remain initialized with vertical tabs"
      );
      for (const atEnd of [false, true]) {
        o((config) => ({
          ...config,
          position_start: atEnd
        }));
        gFlexOrder.applyFlexOrder(atEnd);
        await nextFrame();
        setIsPanelSidebarEnabled(false);
        await nextFrame();
        assertEquals(
          document.getElementById("panel-sidebar-select-box"),
          null,
          "disabling should remove only Floorp's panel sidebar"
        );
        assert(
          document.getElementById("sidebar-container") !== null,
          "disabling Floorp's panel sidebar must preserve Firefox vertical tabs"
        );
        setIsPanelSidebarEnabled(true);
        await nextFrame();
        element("panel-sidebar-select-box");
        element("panel-sidebar-box");
        element(`sidebar-panel-${testPanelId}`);
        assert(
          document.getElementById("sidebar-container") !== null,
          "re-enabling Floorp's panel sidebar must preserve Firefox vertical tabs"
        );
      }
    } finally {
      setIsPanelSidebarEnabled(enabled);
      if (hadVerticalPref) {
        Services.prefs.setBoolPref(verticalPref, savedVerticalPref);
      } else {
        Services.prefs.clearUserPref(verticalPref);
      }
    }
  });
}
async function testFloatingFeatureToggleRestoresBehavior() {
  await withPanel(async () => {
    const enabled = b();
    try {
      o((config) => ({
        ...config,
        floatingWidth: 520,
        floatingHeight: 300,
        floatingPositionLeft: 40,
        floatingPositionTop: 60
      }));
      j(true);
      await nextFrame();
      const originalBox = element("panel-sidebar-box");
      setIsPanelSidebarEnabled(false);
      await nextFrame();
      assertEquals(
        document.getElementById("panel-sidebar-box"),
        null,
        "disabling should unmount the floating panel"
      );
      setIsPanelSidebarEnabled(true);
      await nextFrame();
      const restoredBox = element("panel-sidebar-box");
      assert(
        restoredBox !== originalBox,
        "re-enabling should mount a new floating panel element"
      );
      assertApprox(
        restoredBox.getBoundingClientRect().width,
        520,
        1,
        "re-enabled floating panel should restore its width"
      );
      assertApprox(
        restoredBox.getBoundingClientRect().height,
        300,
        1,
        "re-enabled floating panel should restore its height"
      );
      const header = element("panel-sidebar-header");
      assertEquals(
        header.style.getPropertyValue("cursor"),
        "move",
        "re-enabled floating panel should restore header dragging"
      );
      const before = restoredBox.getBoundingClientRect();
      header.dispatchEvent(
        new MouseEvent("mousedown", {
          bubbles: true,
          clientX: before.left + 10,
          clientY: before.top + 10
        })
      );
      document.dispatchEvent(
        new MouseEvent("mousemove", {
          clientX: before.left + 30,
          clientY: before.top + 25
        })
      );
      await nextFrame();
      document.dispatchEvent(new MouseEvent("mouseup"));
      assert(
        (p().floatingPositionLeft ?? before.left) > before.left,
        "re-enabled floating panel header should remain draggable"
      );
    } finally {
      j(false);
      setIsPanelSidebarEnabled(enabled);
      await nextFrame();
    }
  });
}
async function testHoverAnchor() {
  await withPanel(async () => {
    const root = document.documentElement;
    const launcher = element("sidebar-container");
    const savedRoot = root.hasAttribute("sidebar-expand-on-hover");
    const savedEnd = launcher.hasAttribute("sidebar-positionend");
    const savedExpanded = launcher.hasAttribute("sidebar-launcher-expanded");
    const savedAnimating = launcher.hasAttribute("sidebar-ongoing-animations");
    const pref = "sidebar.position_start";
    const hadPref = Services.prefs.prefHasUserValue(pref);
    const savedPref = Services.prefs.getBoolPref(pref, true);
    const verticalPref = "sidebar.verticalTabs";
    const hadVerticalPref = Services.prefs.prefHasUserValue(verticalPref);
    const savedVerticalPref = Services.prefs.getBoolPref(verticalPref, false);
    const sidebar = globalThis.SidebarController;
    try {
      Services.prefs.setBoolPref(verticalPref, true);
      await nextFrame();
      root.setAttribute("sidebar-expand-on-hover", "");
      for (const firefoxAtStart of [true, false]) {
        Services.prefs.setBoolPref(pref, firefoxAtStart);
        sidebar.setPosition();
        await sidebar.waitUntilStable();
        for (const floorpAtEnd of [false, true]) {
          o((config) => ({
            ...config,
            position_start: floorpAtEnd
          }));
          gFlexOrder.applyFlexOrder(floorpAtEnd);
          for (const open of [true, false]) {
            r(open ? testPanelId : null);
            launcher.removeAttribute("sidebar-launcher-expanded");
            launcher.removeAttribute("sidebar-ongoing-animations");
            await nextFrame();
            const collapsed = launcher.getBoundingClientRect();
            for (const attr of [
              "sidebar-launcher-expanded",
              "sidebar-ongoing-animations"
            ]) {
              launcher.setAttribute(attr, "");
              await nextFrame();
              const expanded = launcher.getBoundingClientRect();
              assertApprox(
                firefoxAtStart ? expanded.left : expanded.right,
                firefoxAtStart ? collapsed.left : collapsed.right,
                1,
                `hover anchor should stay put: Firefox start=${firefoxAtStart}, Floorp end=${floorpAtEnd}, open=${open}, ${attr}`
              );
              launcher.removeAttribute(attr);
              await nextFrame();
              const browser = element("browser");
              const browserRect = browser.getBoundingClientRect();
              const panelRects = [
                "panel-sidebar-select-box",
                "panel-sidebar-box",
                "panel-sidebar-splitter"
              ].flatMap((id) => {
                const panelElement = document.getElementById(id);
                if (!panelElement) return [];
                const style = getComputedStyle(panelElement);
                if (!style || style.display === "none" || style.position === "absolute" || style.position === "fixed") return [];
                return [panelElement.getBoundingClientRect()];
              });
              const occupiedEdge = floorpAtEnd ? Math.min(browserRect.right, ...panelRects.map((r2) => r2.left)) : Math.max(browserRect.left, ...panelRects.map((r2) => r2.right));
              const occupiedWidth = floorpAtEnd ? browserRect.right - occupiedEdge : occupiedEdge - browserRect.left;
              const property = floorpAtEnd ? "--floorp-panel-end-width" : "--floorp-panel-start-width";
              assertApprox(
                parseFloat(browser.style.getPropertyValue(property)),
                occupiedWidth,
                1,
                `hover offset should refresh after ${attr} is removed`
              );
            }
          }
        }
      }
    } finally {
      if (hadPref) Services.prefs.setBoolPref(pref, savedPref);
      else Services.prefs.clearUserPref(pref);
      if (hadVerticalPref) {
        Services.prefs.setBoolPref(verticalPref, savedVerticalPref);
      } else Services.prefs.clearUserPref(verticalPref);
      sidebar.setPosition();
      root.toggleAttribute("sidebar-expand-on-hover", savedRoot);
      launcher.toggleAttribute("sidebar-positionend", savedEnd);
      launcher.toggleAttribute("sidebar-launcher-expanded", savedExpanded);
      launcher.toggleAttribute("sidebar-ongoing-animations", savedAnimating);
      await sidebar.waitUntilStable();
    }
  });
}
function resizeFloatingHandle(id, deltaX, deltaY) {
  const handle = element(id);
  const rect = handle.getBoundingClientRect();
  const clientX = rect.left + rect.width / 2;
  const clientY = rect.top + rect.height / 2;
  handle.dispatchEvent(
    new MouseEvent("mousedown", {
      bubbles: true,
      clientX,
      clientY
    })
  );
  document.dispatchEvent(
    new MouseEvent("mousemove", {
      clientX: clientX + deltaX,
      clientY: clientY + deltaY
    })
  );
  document.dispatchEvent(new MouseEvent("mouseup"));
}
function assertDockedWidth(width) {
  assertEquals(
    PanelNavigator.gPanelSidebar.getPanelData(testPanelId)?.width,
    width,
    "floating resize must preserve the docked width, including the global-width sentinel"
  );
  const saved = JSON.parse(
    Services.prefs.getStringPref("floorp.panelSidebar.data")
  );
  assertEquals(
    saved.data.find((panel) => panel.id === testPanelId)?.width,
    width,
    "persisted docked width should remain independent of floating dimensions"
  );
}
async function testFloatingResizePersists() {
  await withPanel(async () => {
    o((config) => ({
      ...config,
      floatingWidth: 500,
      floatingHeight: 300
    }));
    j(true);
    await nextFrame();
    const box = element("panel-sidebar-box");
    const before = box.getBoundingClientRect();
    resizeFloatingHandle("floating-splitter-right", 60, 0);
    const width = box.getBoundingClientRect().width;
    assertApprox(
      width,
      before.width + 60,
      1,
      "floating resize should flush the final frame"
    );
    assertApprox(
      p().floatingWidth ?? 0,
      width,
      1,
      "floating configuration should persist the final width"
    );
    assertDockedWidth(400);
    resizeFloatingHandle("floating-splitter-corner-bottomright", 20, 30);
    assertApprox(
      p().floatingWidth ?? 0,
      width + 20,
      1,
      "diagonal resize should save floating width"
    );
    assertApprox(
      p().floatingHeight ?? 0,
      before.height + 30,
      1,
      "diagonal resize should save floating height"
    );
    assertDockedWidth(400);
    await nextFrame();
    const browser = element("browser");
    const property = p().position_start ? "--floorp-panel-end-width" : "--floorp-panel-start-width";
    assertApprox(
      parseFloat(browser.style.getPropertyValue(property)),
      element("panel-sidebar-select-box").getBoundingClientRect().width,
      1,
      "floating panel should not reserve space for the hover launcher"
    );
  });
}
async function testVerticalFloatingResizePreservesGlobalWidth() {
  await withPanel(async () => {
    const controller = PanelNavigator.gPanelSidebar;
    q(
      (panels) => panels.map(
        (panel) => panel.id === testPanelId ? { ...panel, width: 0 } : panel
      )
    );
    o((config) => ({
      ...config,
      globalWidth: 400,
      floatingWidth: 520,
      floatingHeight: 300
    }));
    j(true);
    await nextFrame();
    resizeFloatingHandle("floating-splitter-bottom", 0, 40);
    assertDockedWidth(0);
    assertApprox(
      p().floatingWidth ?? 0,
      520,
      1,
      "vertical resize should keep the floating width"
    );
    assertApprox(
      p().floatingHeight ?? 0,
      340,
      1,
      "vertical resize should persist the final height"
    );
    j(false);
    o((config) => ({ ...config, globalWidth: 450 }));
    controller.changePanel(testPanelId);
    controller.changePanel(testPanelId);
    await nextFrame();
    assertApprox(
      element("panel-sidebar-box").getBoundingClientRect().width,
      450,
      1,
      "default panel should continue following global width"
    );
    j(true);
    await nextFrame();
    assertApprox(
      element("panel-sidebar-box").getBoundingClientRect().width,
      520,
      1,
      "floating width should remain independent of global width"
    );
    assertDockedWidth(0);
  });
}
async function testModeSwitchRestoresIndependentWidths() {
  await withPanel(async () => {
    const controller = PanelNavigator.gPanelSidebar;
    o((config) => ({
      ...config,
      floatingWidth: 560,
      floatingHeight: 320
    }));
    j(true);
    await nextFrame();
    controller.changePanel(testPanelId);
    controller.changePanel(testPanelId);
    await nextFrame();
    assertApprox(
      element("panel-sidebar-box").getBoundingClientRect().width,
      560,
      1,
      "reopening a floating panel should use floating width"
    );
    j(false);
    await nextFrame();
    assertApprox(
      element("panel-sidebar-box").getBoundingClientRect().width,
      400,
      1,
      "docking should restore the panel's saved docked width"
    );
    assertDockedWidth(400);
    j(true);
    await nextFrame();
    assertApprox(
      element("panel-sidebar-box").getBoundingClientRect().width,
      560,
      1,
      "floating again should restore floating width"
    );
  });
}
async function runAllTests() {
  await runTests("panelSidebarLayout.test.ts", [
    {
      name: "embedded web panel viewport fits its container",
      fn: testWebPanelViewport
    },
    {
      name: "completed resize persists across closing and recreating panels",
      fn: testCompletedResizePersists
    },
    {
      name: "feature toggling preserves Floorp panels and Firefox vertical tabs",
      fn: testFeatureToggleWithVerticalTabs
    },
    {
      name: "feature toggling restores floating panel behavior",
      fn: testFloatingFeatureToggleRestoresBehavior
    },
    {
      name: "hover expansion preserves the anchor on either side with panels open or closed",
      fn: testHoverAnchor
    },
    {
      name: "floating resize flushes and persists the final animation frame",
      fn: testFloatingResizePersists
    },
    {
      name: "vertical floating resize preserves the global-width sentinel",
      fn: testVerticalFloatingResizePreservesGlobalWidth
    },
    {
      name: "mode switches restore independent docked and floating widths",
      fn: testModeSwitchRestoresIndependentWidths
    }
  ]);
}
export {
  runAllTests
};
