// SPDX-License-Identifier: MPL-2.0
import { observeSponsorOperation } from "./corpus-sponsor-reference.mjs";
/** @param {import("./network-test-types.ts").SponsorFloorpDriver} driver */
export async function runSponsorFloorp(driver) {
  await driver.pause(2500);
  driver.item.lateOnboardingTabs = [];
  for (const tab of [...driver.window.gBrowser.tabs]) {
    if (tab === driver.tab) continue;
    const url = tab.linkedBrowser.currentURI.spec;
    if (
      !url.startsWith(driver.extensionBaseURL) &&
      !url.startsWith("https://sponsor.ajay.app/")
    )
      continue;
    driver.item.lateOnboardingTabs.push({ url });
    driver.window.gBrowser.removeTab(tab, { skipPermitUnload: true });
  }
  driver.window.gBrowser.selectedTab = driver.tab;
  const snapshot = () => driver.query("Snapshot");
  return observeSponsorOperation({
    ...driver,
    snapshot,
    async play() {
      await driver.until(async () => {
        const value = await snapshot();
        return (
          value.ready === "complete" &&
          value.video &&
          value.video.readyState >= 2 &&
          value
        );
      }, "native decoded SponsorBlock fixture video");
      await driver.query("Click", { selector: "#play" });
      return driver.until(async () => {
        const value = await snapshot();
        return (
          value.video &&
          !value.video.paused &&
          value.video.currentTime > 0.1 &&
          value
        );
      }, "real native playback for SponsorBlock");
    },
  });
}
