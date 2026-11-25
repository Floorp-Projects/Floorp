// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import {
  HUB_MENU_FEATURE_PREF_DEFAULT,
  HUB_MENU_MANUAL_HOST_PREF_DEFAULT,
  selectHubPanelMenuStartupMode,
} from "../../browser-features/modules/modules/feature-host/HubPanelMenuStartupSelection.ts";

Deno.test("Hub menu startup selection truth table", () => {
  const cases = [
    {
      startupMode: "dev",
      featureEnabled: false,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "dev",
      featureEnabled: false,
      manualHostEnabled: true,
      expected: "legacy",
    },
    {
      startupMode: "dev",
      featureEnabled: true,
      manualHostEnabled: false,
      expected: "pinned",
    },
    {
      startupMode: "dev",
      featureEnabled: true,
      manualHostEnabled: true,
      expected: "manual",
    },
    {
      startupMode: "test",
      featureEnabled: false,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "test",
      featureEnabled: false,
      manualHostEnabled: true,
      expected: "legacy",
    },
    {
      startupMode: "test",
      featureEnabled: true,
      manualHostEnabled: false,
      expected: "pinned",
    },
    {
      startupMode: "test",
      featureEnabled: true,
      manualHostEnabled: true,
      expected: "manual",
    },
    {
      startupMode: "production",
      featureEnabled: false,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "production",
      featureEnabled: false,
      manualHostEnabled: true,
      expected: "legacy",
    },
    {
      startupMode: "production",
      featureEnabled: true,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "production",
      featureEnabled: true,
      manualHostEnabled: true,
      expected: "legacy",
    },
    {
      startupMode: "",
      featureEnabled: false,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "",
      featureEnabled: false,
      manualHostEnabled: true,
      expected: "legacy",
    },
    {
      startupMode: "",
      featureEnabled: true,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "",
      featureEnabled: true,
      manualHostEnabled: true,
      expected: "legacy",
    },
    {
      startupMode: "unknown",
      featureEnabled: false,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "unknown",
      featureEnabled: false,
      manualHostEnabled: true,
      expected: "legacy",
    },
    {
      startupMode: "unknown",
      featureEnabled: true,
      manualHostEnabled: false,
      expected: "legacy",
    },
    {
      startupMode: "unknown",
      featureEnabled: true,
      manualHostEnabled: true,
      expected: "legacy",
    },
  ] as const;

  for (const testCase of cases) {
    assertEquals(
      selectHubPanelMenuStartupMode(
        testCase.startupMode,
        testCase.featureEnabled,
        testCase.manualHostEnabled,
      ),
      testCase.expected,
      `${testCase.startupMode}/${testCase.featureEnabled}/${testCase.manualHostEnabled}`,
    );
  }
});

Deno.test("Hub menu startup preferences default to disabled", () => {
  assertEquals(HUB_MENU_FEATURE_PREF_DEFAULT, false);
  assertEquals(HUB_MENU_MANUAL_HOST_PREF_DEFAULT, false);
  assertEquals(
    selectHubPanelMenuStartupMode(
      "dev",
      HUB_MENU_FEATURE_PREF_DEFAULT,
      HUB_MENU_MANUAL_HOST_PREF_DEFAULT,
    ),
    "legacy",
  );
  assertEquals(
    selectHubPanelMenuStartupMode(
      "dev",
      true,
      HUB_MENU_MANUAL_HOST_PREF_DEFAULT,
    ),
    "pinned",
  );
});
