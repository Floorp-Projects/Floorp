// SPDX-License-Identifier: MPL-2.0
export type UIVisionSettingsControl =
  | "welcome"
  | "websiteRuns"
  | "editWhitelist"
  | "whitelistText"
  | "saveWhitelist"
  | "dataTab"
  | "logsTab";

export interface UIVisionInputEvent {
  type: string;
  trusted: boolean;
}

export interface UIVisionSettingsControlState {
  control: UIVisionSettingsControl;
  url: string;
  ready: string;
  present: boolean;
  visible: boolean;
  enabled: boolean;
  unobstructed: boolean;
  checked: boolean | null;
  value: string | null;
  title: string | null;
  x: number;
  y: number;
  width: number;
  height: number;
  selected?: boolean;
  logViewVisible?: boolean;
  events?: UIVisionInputEvent[];
}

export interface UIVisionWebsiteConfiguration {
  allowRunFromHttpSchema?: boolean;
  websiteInvokePolicyVersion?: number;
  websiteWhiteList?: string[];
}

export interface UIVisionTrustedSettingsResult {
  panelSession: string;
  onboarding: {
    shown: boolean;
    clicked: boolean;
    title: string | null;
    events: UIVisionInputEvent[];
  };
  configuration: {
    method: "actual-product-settings-native-CDP-input";
    settingsURL: string;
    before: UIVisionWebsiteConfiguration;
    after: UIVisionWebsiteConfiguration;
    steps: UIVisionSettingsControlState[];
  };
}

export interface UIVisionInvocationObservation {
  transport?: "Runtime.consoleAPICalled";
  dispatchError?: string;
  witnessContextEnded?: boolean;
  cleanupErrors?: string[];
  dispatchedDocumentedBridge: boolean;
  witnessMs: number;
  events: { type: string; message?: string }[];
}

export interface UIVisionSettingsFailure {
  settingsURL: string;
  steps: UIVisionSettingsControlState[];
  snapshot?: {
    url: string;
    ready: string;
    title: string;
    text: string;
    activeElement: {
      tag: string;
      id: string;
      type: string | null;
      placeholder: string | null;
    } | null;
    controls: UIVisionSettingsControlState[];
    configuration: UIVisionWebsiteConfiguration;
  };
  snapshotError?: string;
  screenshot?: { path: string; sha256: string; bytes: number };
  screenshotError?: string;
}

export interface UIVisionLogViewFailure {
  panelURL: string;
  steps: UIVisionSettingsControlState[];
  snapshot?: {
    url: string;
    ready: string;
    title: string;
    text: string;
    controls: UIVisionSettingsControlState[];
  };
  snapshotError?: string;
  screenshot?: { path: string; sha256: string; bytes: number };
  screenshotError?: string;
}
