// SPDX-License-Identifier: MPL-2.0
// Observe real decoded media time. No product or YouTube APIs are replaced.
/** @param {import("./network-test-types.ts").SponsorDriver} driver */
export async function observeSponsorOperation(driver) {
  const { item, snapshot, until, play, navigate, url } = driver;
  /** @param {string} label */
  async function observeSkip(label) {
    await play();
    const started = Date.now();
    /** @type {{elapsed: number, video: import("./network-test-types.ts").MediaSnapshot["video"]}[]} */
    const samples = [];
    const result = await until(async () => {
      const value = await snapshot();
      const elapsed = (Date.now() - started) / 1000;
      samples.push({ elapsed, video: value.video });
      item.lastSnapshot = value;
      if (elapsed > 5) {
        throw Error(
          "No real SponsorBlock seek before natural playback could reach the authored segment end",
        );
      }
      if (value.video?.error) throw Error("Native media decoding failed");
      return (
        value.video &&
        !value.video.paused &&
        value.video.currentTime >= 7 &&
        value.video.currentTime > elapsed + 3 && { ...value, samples }
      );
    }, label);
    return result;
  }
  item.transformed = await observeSkip(
    "actual SponsorBlock native media seek across authored sponsor segment",
  );
  item.checks.push(
    "Unchanged SponsorBlock skips the real HTMLVideoElement from the authored [2,7] sponsor interval before natural playback can reach its end",
  );
  await navigate(url + "&second=1");
  item.afterNavigation = await observeSkip(
    "SponsorBlock native seek after real document navigation",
  );
  item.checks.push(
    "Actual new-document navigation retains functioning sponsor skipping",
  );
  return async function verifyRemoval() {
    await play();
    const started = Date.now();
    item.afterRemoval = await until(async () => {
      const value = await snapshot();
      const elapsed = (Date.now() - started) / 1000;
      if (value.video && value.video.currentTime >= 6) {
        throw Error("Sponsor skipping remained after actual uninstall");
      }
      if (elapsed > 5)
        throw Error("Native negative-control playback did not advance");
      return (
        value.video &&
        !value.video.paused &&
        value.video.currentTime >= 2.6 &&
        value.video.currentTime < 4.5 &&
        value
      );
    }, "native media plays through sponsor interval after uninstall");
    item.checks.push(
      "Actual uninstall and new navigation restore continuous native playback through the sponsor interval",
    );
  };
}
/** @param {import("./network-test-types.ts").SponsorReferenceDriver} driver */
export async function runSponsorReference(driver) {
  // Its asynchronous install handler opens a real help tab after settings load.
  // Close that product-owned tab before starting the timed native-media test.
  await driver.delay(2500);
  driver.item.lateOnboardingTargets = [];
  for (const target of (await driver.browser.call("Target.getTargets"))
    .targetInfos) {
    if (
      target.type !== "page" ||
      target.targetId === driver.targetId ||
      !(
        target.url.startsWith(`chrome-extension://${driver.item.id}/`) ||
        target.url.startsWith("https://sponsor.ajay.app/")
      )
    )
      continue;
    driver.item.lateOnboardingTargets.push(target);
    await driver.browser.call("Target.closeTarget", {
      targetId: target.targetId,
    });
  }
  return observeSponsorOperation({
    ...driver,
    play: () => driver.readyAndPlay(),
  });
}
