// SPDX-License-Identifier: MPL-2.0
export interface Tab {
  url: string;
  workspace: string | null;
  hidden: boolean;
  selected: boolean;
}

export interface RestoreState {
  configuredNewTabURL: string;
  pendingExit: boolean;
  tabs: Tab[];
}

export interface Preparation {
  state: { configuredNewTabURL: string };
  retainedURL: string;
  closedURL: string;
  b: string;
}

export interface SharedOracle {
  configuration(
    state: { configuredNewTabURL: string },
    expected: { configuredNewTabURL: string },
  ): void;
  workspaceExit(
    state: RestoreState,
    expected: Preparation,
    homepage: string,
  ): void;
  selectedWorkspace(
    after: RestoreState,
    before: RestoreState,
    expected: Preparation,
    homepage: string,
  ): void;
}

export interface PhaseResult {
  ok: boolean;
  syntheticProfileVerified: boolean;
  error?: string;
  observedState?: {
    configuredNewTabURL: string;
    tabs: Array<
      { currentURI: string | null; workspace: string | null; selected: boolean }
    >;
  };
}
