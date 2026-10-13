// SPDX-License-Identifier: MPL-2.0

import { nativeHiddenTabTests } from "../../browser-features/chrome/common/workspaces/test/workspace-last-tab-test-utils.ts";
import {
  beginAssertions,
  endAssertions,
  firstCaseException,
} from "./observed_harness.ts";
import { CASES } from "./cases.ts";
import type { BrowserIdentity, BrowserResult, CaseResult } from "./types.ts";

interface ProductWindow {
  workspacesFuncs?: unknown;
  Services: {
    appinfo: { appBuildID: string; platformBuildID: string; version: string };
    prefs: {
      getStringPref(name: string, fallback?: string): string;
      getBoolPref(name: string, fallback?: boolean): boolean;
    };
    io: { newURI(value: string): unknown };
  };
  Cc: Record<
    string,
    {
      getService(
        iid: unknown,
      ): { convertChromeURL(uri: unknown): { spec: string } };
    }
  >;
  Ci: { nsIChromeRegistry: unknown };
}

const scope = globalThis as unknown as ProductWindow & {
  __QA3_WORKSPACE_PILOT__?: { run(nonce: string): Promise<BrowserResult> };
};

if (scope.__QA3_WORKSPACE_PILOT__ !== undefined) {
  throw new Error("Workspace pilot payload was injected more than once");
}
let executed = false;

scope.__QA3_WORKSPACE_PILOT__ = Object.freeze({
  async run(nonce: string): Promise<BrowserResult> {
    if (executed || !/^[0-9a-f-]{36}$/.test(nonce)) {
      throw new Error("Duplicate or invalid workspace pilot execution");
    }
    executed = true;
    const services = scope.Services;
    const bootstrap: BrowserIdentity = {
      appBuildID: services.appinfo.appBuildID,
      platformBuildID: services.appinfo.platformBuildID,
      version: services.appinfo.version,
      coreResource: "chrome://noraneko/content/core.js",
      resolvedCoreURL:
        scope.Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
          scope.Ci.nsIChromeRegistry,
        ).convertChromeURL(
          services.io.newURI("chrome://noraneko/content/core.js"),
        ).spec,
      startupMode: services.prefs.getStringPref("nora.startup.mode", ""),
      httpLoaderAllowed: services.prefs.getBoolPref(
        "nora.dev.allow_http_loader",
        false,
      ),
      workspaceAPI: Boolean(scope.workspacesFuncs),
    };
    if (
      bootstrap.startupMode !== "production" || bootstrap.httpLoaderAllowed ||
      !bootstrap.workspaceAPI
    ) {
      throw new Error(
        "Packaged production bootstrap and workspace API are required",
      );
    }
    const tests = nativeHiddenTabTests(true);
    if (
      tests.length !== CASES.length ||
      tests.some((t, i) => t.name !== CASES[i].name)
    ) {
      throw new Error("Source-derived workspace selection changed");
    }
    const cases: CaseResult[] = [];
    let failed = false;
    for (const [index, required] of CASES.entries()) {
      if (failed) {
        cases.push({
          id: required.id,
          name: required.name,
          status: "NOT_RUN",
          assertions: [],
          originalError: null,
          error: "Earlier required case failed",
        });
        continue;
      }
      const assertions = beginAssertions();
      let error: string | null = null;
      try {
        await tests[index].fn();
      } catch (caught) {
        error = String(caught).slice(0, 1024);
      } finally {
        endAssertions();
      }
      const firstFailedAssertion = assertions.find((event) => !event.passed);
      if (firstFailedAssertion) error ??= firstFailedAssertion.message;
      if (assertions.length < required.minimumAssertions) {
        error ??= "Required positive assertion count was not reached";
      }
      const originalError = firstCaseException();
      failed = error !== null || originalError !== null;
      cases.push({
        id: required.id,
        name: required.name,
        status: failed ? "FAIL" : "PASS",
        assertions,
        originalError,
        error,
      });
    }
    return {
      schema: "qa3-final-workspace-browser-v1",
      nonce,
      bootstrap,
      cases,
    };
  },
});
