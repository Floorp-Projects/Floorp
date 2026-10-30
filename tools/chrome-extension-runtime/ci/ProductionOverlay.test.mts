// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
/// <reference path="../../../libs/@types/gecko/index.d.ts" />
import {
  assert,
  assertEquals,
} from "../../../browser-features/chrome/test/utils/test_harness.ts";
import { runAllTests as testUpdater } from "../updater-import-smoke.mts";

export async function runAllTests(): Promise<void> {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs",
  );
  assert(
    AppConstants.MOZ_WEBEXT_WEBIDL_ENABLED,
    "This is a freshly compiled native WebExtension binding candidate",
  );
  assertEquals(
    Services.prefs.getStringPref("nora.startup.mode", ""),
    "production",
    "Actual Floorp production startup executes",
  );
  assert(
    !Services.prefs.getBoolPref("nora.dev.allow_http_loader", false),
    "Production browser does not permit the development HTTP loader",
  );
  const { NetUtil } = ChromeUtils.importESModule(
    "resource://gre/modules/NetUtil.sys.mjs",
  );
  for (const uri of [
    "chrome://noraneko/content/core.js",
    "chrome://noraneko-startup/content/chrome_root.js",
    "chrome://noraneko-settings/content/index.html",
    "chrome://noraneko-newtab/content/index.html",
  ]) {
    const bytes = await new Promise<number>((resolve, reject) => {
      NetUtil.asyncFetch(
        { uri, loadUsingSystemPrincipal: true },
        (stream: nsIInputStream, status: number) => {
          if (!Components.isSuccessCode(status)) {
            reject(
              new Error(`Packaged Floorp resource is unavailable: ${uri}`),
            );
            return;
          }
          const size = stream.available();
          stream.close();
          resolve(size);
        },
      );
    });
    assert(bytes > 0, `Production package contains ${uri}`);
  }
  await testUpdater();
}
