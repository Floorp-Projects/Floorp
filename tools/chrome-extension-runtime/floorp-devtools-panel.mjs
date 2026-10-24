// SPDX-License-Identifier: MPL-2.0
// Test adapter for the real Gecko toolbox and real installed extension panels.
/** @import {Truthy,NativeTabHandle,NativeExtensionHandle,NativeToolboxModule,NativeFloorpFrontend} from "./assessment-test-types.ts" */
const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const { gDevTools } = /** @type {NativeToolboxModule} */ (require(
  "devtools/client/framework/devtools.js",
));
/** @param {number} ms @returns {Promise<void>} */
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @template T @param {()=>T|Promise<T>} read @param {string} label @param {number} [timeout] @returns {Promise<Truthy<T>>} */
async function waitFor(read, label, timeout = 30000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    const result = await read();
    if (result) return /** @type {Truthy<T>} */ (result);
    await delay(100);
  }
  throw Error("Timed out waiting for " + label);
}
/** @param {NativeTabHandle} tab @param {NativeExtensionHandle} extension @returns {Promise<NativeFloorpFrontend>} */
export async function attachNativeFloorpDevtools(tab, extension) {
  const openedToolbox = await gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
    hostType: "bottom",
    raise: true,
  });
  if (!openedToolbox) throw Error("The native toolbox did not open");
  const toolbox = openedToolbox;
  const panels = () =>
    toolbox.getAdditionalTools().filter(
      (definition) => definition.extensionId === extension.id,
    );
  /** @param {string} title @param {string} [path] */
  async function selectPanel(title, path) {
    const definition = await waitFor(
      () => panels().find((definition) => definition.label === title),
      "real registered extension toolbox panel " + title,
    );
    await toolbox.selectTool(definition.id, "extension-corpus-test");
    const view = await waitFor(
      () =>
        [...extension.views].find((view) =>
          view.viewType === "devtools_panel" && !view.unloaded &&
          view.devToolsToolbox === toolbox && view.xulBrowser &&
          (!path || view.uri?.spec.includes(path))
        ),
      "loaded native extension panel " + title,
    );
    if (toolbox.currentToolId !== definition.id) {
      throw Error("The requested native extension panel was not selected");
    }
    if (!view.xulBrowser) {
      throw Error("Native extension panel browser is missing");
    }
    return { definition, view, browser: view.xulBrowser };
  }
  return { toolbox, panels, selectPanel, close: () => toolbox.destroy() };
}
