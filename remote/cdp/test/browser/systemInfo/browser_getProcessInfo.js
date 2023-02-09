/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(
  async function getProcessInfoDetails({ client }) {
    const { SystemInfo } = client;

    const processInfo = await SystemInfo.getProcessInfo();
    assertProcesses(processInfo);
  },
  { createTab: false }
);

add_task(
  async function getProcessInfoMultipleTabs({ client }) {
    const { SystemInfo, Target } = client;

    const { newTab: newTab1 } = await openTab(Target);
    const { newTab: newTab2 } = await openTab(Target);
    const { newTab: newTab3 } = await openTab(Target);
    const { newTab: newTab4 } = await openTab(Target);

    const processInfo = await SystemInfo.getProcessInfo();
    assertProcesses(processInfo, [newTab1, newTab2, newTab3, newTab4]);
  },
  { createTab: false }
);

add_task(
  async function getProcessInfoMultipleWindows({ client }) {
    const { SystemInfo, Target } = client;

    const { newWindow: newWindow1 } = await openWindow(Target);
    const { newWindow: newWindow2 } = await openWindow(Target);

    const processInfo = await SystemInfo.getProcessInfo();
    assertProcesses(processInfo, [
      ...newWindow1.gBrowser.tabs,
      ...newWindow2.gBrowser.tabs,
    ]);

    await BrowserTestUtils.closeWindow(newWindow1);
    await BrowserTestUtils.closeWindow(newWindow2);
  },
  { createTab: false }
);

function assertProcesses(processInfo, tabs) {
  ok(Array.isArray(processInfo), "Process info is an array");

  for (const info of processInfo) {
    ok(typeof info.id === "number", "Info has a numeric id");
    ok(typeof info.type === "string", "Info has a string type");
    ok(typeof info.cpuTime === "number", "Info has a numeric cpuTime");
  }

  const getByType = type => processInfo.filter(info => info.type === type);

  is(
    getByType("browser").length,
    1,
    "Got expected amount of browser processes"
  );
  ok(!!getByType("renderer").length, "Got at least one renderer process");

  if (tabs) {
    const rendererPids = new Set(
      processInfo.filter(info => info.type === "renderer").map(info => info.id)
    );

    for (const tab of tabs) {
      const pid = tab.linkedBrowser.browsingContext.currentWindowGlobal.osPid;
      ok(rendererPids.has(pid), `Found process info for pid (${pid})`);
    }
  }
}
