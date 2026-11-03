// SPDX-License-Identifier: MPL-2.0

export async function runAllTests(): Promise<void> {
  const { ChromeExtensionUpdateScheduler } = ChromeUtils.importESModule(
    "resource://noraneko/modules/chrome-web-store/ChromeExtensionUpdateScheduler.sys.mjs",
  );
  if (
    typeof ChromeExtensionUpdateScheduler?.getStatus !== "function" ||
    typeof ChromeExtensionUpdateScheduler?.setAutomaticUpdates !== "function"
  ) {
    throw new Error("The production automatic-update scheduler did not load");
  }
  const status = await ChromeExtensionUpdateScheduler.getStatus(
    "not-a-converted-addon",
  );
  if (status.eligibility !== "unavailable" || status.enabled) {
    throw new Error("A missing addon was enrolled for automatic updates");
  }
  const {
    ChromeExtensionUpdater,
    downloadChromeExtension,
    getChromeDownloadTarget,
  } = ChromeUtils.importESModule(
    "resource://noraneko/modules/chrome-web-store/ChromeExtensionUpdater.sys.mjs",
  );
  if (
    typeof ChromeExtensionUpdater?.checkForUpdate !== "function" ||
    typeof ChromeExtensionUpdater?.installUpdate !== "function" ||
    typeof downloadChromeExtension !== "function" ||
    typeof getChromeDownloadTarget !== "function"
  ) {
    throw new Error("The production Chrome extension updater did not load");
  }
  const target = getChromeDownloadTarget();
  if (!target.os || !target.arch) {
    throw new Error("The production download target is unavailable");
  }
  let rejection: string | undefined;
  try {
    await ChromeExtensionUpdater.checkForUpdate("not-a-converted-addon");
  } catch (error) {
    rejection = String(error);
  }
  if (
    !rejection || !/converted|Chrome|installed|identifier|ID/i.test(rejection)
  ) {
    throw new Error(`Invalid extension ID was not rejected: ${rejection}`);
  }
}
