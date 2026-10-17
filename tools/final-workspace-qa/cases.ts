// SPDX-License-Identifier: MPL-2.0

import type { BrowserResult, PilotCase } from "./types.ts";

export const CASES: readonly PilotCase[] = Object.freeze(
  [false, true].flatMap((close) =>
    ["extension", "unattributed", "collapsed-group"].map((kind, index) => ({
      id: `workspace-native-${close ? "true" : "false"}-${kind}`,
      name:
        `${kind} tab retains native behavior (workspaces true, native ${close})`,
      minimumAssertions: close ? [11, 10, 11][index] : [12, 11, 11][index],
    }))
  ).map((item) => Object.freeze(item)),
);

export function verifyBrowserResult(
  result: BrowserResult,
  nonce: string,
  expected: { appBuildID: string; platformBuildID: string; version: string },
): void {
  const exact = (value: unknown, keys: string[]) => {
    if (
      typeof value !== "object" || value === null || Array.isArray(value) ||
      JSON.stringify(Object.keys(value).sort()) !== JSON.stringify(keys.sort())
    ) {
      throw new Error(
        "Original browser result is incomplete or has unexpected fields",
      );
    }
  };
  exact(result, ["schema", "nonce", "bootstrap", "cases"]);
  exact(result.bootstrap, [
    "appBuildID",
    "platformBuildID",
    "version",
    "coreResource",
    "resolvedCoreURL",
    "startupMode",
    "httpLoaderAllowed",
    "workspaceAPI",
  ]);
  if (
    result.schema !== "qa3-final-workspace-browser-v1" ||
    result.nonce !== nonce ||
    result.bootstrap.startupMode !== "production" ||
    result.bootstrap.httpLoaderAllowed !== false ||
    result.bootstrap.workspaceAPI !== true ||
    result.bootstrap.coreResource !== "chrome://noraneko/content/core.js" ||
    !/^jar:file:\/\/\/input\/browser\/[^?#]+!\/|^file:\/\/\/input\/browser\//
      .test(result.bootstrap.resolvedCoreURL) ||
    result.bootstrap.appBuildID !== expected.appBuildID ||
    result.bootstrap.platformBuildID !== expected.platformBuildID ||
    result.bootstrap.version !== expected.version
  ) {
    throw new Error(
      "Product bootstrap or original execution identity mismatch",
    );
  }
  if (result.cases.length !== CASES.length) {
    throw new Error("Required workspace case set is incomplete");
  }
  for (const [index, required] of CASES.entries()) {
    const actual = result.cases[index];
    exact(actual, [
      "id",
      "name",
      "status",
      "assertions",
      "originalError",
      "error",
    ]);
    for (const event of actual.assertions) exact(event, ["message", "passed"]);
    if (
      actual.id !== required.id || actual.name !== required.name ||
      actual.status !== "PASS" || actual.error !== null ||
      actual.originalError !== null ||
      actual.assertions.length < required.minimumAssertions ||
      actual.assertions.some((event) =>
        event.passed !== true || typeof event.message !== "string" ||
        event.message.length === 0 || event.message.length > 512
      )
    ) {
      throw new Error(
        `Required case failed or lacked assertions: ${required.id}`,
      );
    }
  }
}
