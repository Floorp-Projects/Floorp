// SPDX-License-Identifier: MPL-2.0

export type FailureStateScenario = {
  name: "invalid-canonical-state" | "persisted-quarantine";
  profileName: string;
  stateBytes: Uint8Array;
};

export interface StartupSnapshot {
  delayedStartupFinished: boolean;
  mode: string;
  selected: boolean;
  isPrivate: boolean;
  windowType: string;
  popupOpen: boolean;
  buttonPresent: boolean;
  settingsPresent: boolean;
}

export interface RegistrySnapshot {
  connections: number;
  staged: boolean;
  published: boolean;
  enabled: boolean;
  pendingAcknowledgements: number;
}

export interface MenuSnapshot {
  role: string;
  isPrivate: boolean;
  delayedStartupFinished: boolean;
  popupOpen: boolean;
  settingsPresent: boolean;
  buttonMatchCount: number;
  button: null | {
    id: string;
    tag: string;
    className: string;
    label: string;
    disabled: boolean | null;
    immediateSettingsSibling: boolean;
    matchCount: number;
  };
}

export interface HostRecord {
  featureId: string;
  desiredEnabled: boolean;
  selectedVersion: string | null;
  selectedHealth?: string | null;
  rollbackVersion: string | null;
  stateRevision: number;
  blockedReason: string | null;
  lastOutcome:
    | null
    | { txId: string; status: string; selectedVersion?: string | null };
}

export interface HostSnapshot {
  ready: boolean;
  error: string | null;
  diagnostics: HostRecord | null;
  windowIds: string[];
  trackedWindows: number;
}

export interface HostOperationSnapshot {
  done: boolean;
  error: string | null;
  txId: string;
  expectedRevision: number;
  operation: string;
  target: string | null;
  outcome:
    | { txId: string; status: string; selectedVersion: string | null }
    | null;
  queriedOutcome: unknown;
  before: HostRecord;
  after: HostRecord | null;
}

export interface CommandProbeSnapshot {
  role: string;
  tabCount: number;
  popupOpen: boolean;
  hideCalls: number;
  retainedConnected: boolean;
  calls: {
    url: string;
    relatedToCurrent: boolean;
    systemPrincipal: boolean;
    hasReturnedTab: boolean;
    returnedTabSelected: boolean;
    currentUrl: string;
  }[];
}

export interface FocusSnapshot {
  role: string;
  windowFocused: boolean;
  activeWindowIsTarget: boolean;
  activeWindowTitle: string;
  popupOpen: boolean;
  activeElementId: string;
  activeElementTag: string;
  activeButton: boolean;
  buttonMatchesFocusPseudo: boolean;
  buttonTabIndex: number | null;
  buttonTabIndexAttribute: string | null;
  dispatcherFocusedId: string;
  dispatcherFocusedTag: string;
  serviceFocusedId: string;
  serviceFocusedTag: string;
  activeSettings: boolean;
  activeTrigger: boolean;
  buttonMatchCount: number;
  buttonEnabled: boolean | null;
  settingsVisible: boolean;
  triggerVisible: boolean;
}

export interface FocusMovementItemSnapshot {
  id: string;
  tag: string;
  active: boolean;
  connected: boolean;
  enabled: boolean;
  insideMenu: boolean;
  visible: boolean;
  tabIndex: number;
  tabIndexAttribute: string | null;
}

export interface AttachCompletionGateSnapshot {
  windowId: string;
  armedAt: number;
  entered: boolean;
  enteredAt: number | null;
  releasedAt: number | null;
}

export interface HostShutdownSnapshot {
  ready: boolean;
  error: string | null;
  root: string;
  errors: string[];
  timedOut: boolean;
  windowIds: string[];
  windowLifecycle: {
    observers: boolean;
    tracked: number;
    pendingStartup: number;
    readyWaiters: number;
    admissionTasks: number;
    pendingDetaches: number;
    cleanupFailures: number;
  };
  scope: {
    resources: number;
    childScopes: number;
    tasks: number;
    pendingTasks: number;
    observedTaskRejections: number;
    taskFailures: number;
  };
  registryBefore: RegistrySnapshot;
  registry: RegistrySnapshot;
}

export interface HostWindowIdentitySnapshot {
  hostWindowIds: string[];
  roleWindowIds: Record<string, string | null>;
}

export interface ViteProbeSnapshot {
  role: string;
  windowId: string | null;
  windowFocused: boolean;
  popupOpen: boolean;
  buttonMatchCount: number;
  button: null | {
    id: string;
    className: string;
    disabled: boolean | null;
    immediateSettingsSibling: boolean;
  };
  initialButtonConnected: boolean;
  mutationButtonConnected: boolean;
  replacedInitialButton: boolean;
  replacedMutationButton: boolean;
}

export interface ViteUpdateLogSnapshot {
  updates: string[];
  reloads: string[];
}

export interface ResilienceSnapshot {
  registry: RegistrySnapshot;
  host: HostSnapshot | null;
  identity: HostWindowIdentitySnapshot | null;
  canonical: HostRecord | null;
}

export interface Scenario {
  name: "opt-in" | "opt-out" | FailureStateScenario["name"];
  selected: boolean;
}

export type PhaseStatus = "PASS" | "FAIL" | "NOT_RUN" | "NOT_APPLICABLE";

export interface ScenarioPhaseVerdict {
  scenario: Scenario["name"];
  canonicalHostCases: PhaseStatus;
  isolatedPendingMovement: PhaseStatus;
}

export type AcceptanceMode =
  | "startup"
  | "lifecycle"
  | "hmr-only"
  | "failure-states";

export interface StateFileSnapshot {
  path: string;
  contents: string | null;
}
