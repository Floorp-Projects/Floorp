// SPDX-License-Identifier: MPL-2.0
export interface CorpusSelection {
  id: string;
  name: string;
  area: string;
  primaryOperation: string;
  acceptedManifestNames?: string[];
}

export interface FrozenCorpusExtension extends CorpusSelection {
  status: "frozen" | "unavailable";
  version?: string;
  packageSha256?: string;
  packageBytes?: number;
  signature?: { publicKeySha256: string };
}
export interface FrozenCorpus {
  extensions: FrozenCorpusExtension[];
}

export interface CorpusUISnapshot {
  url: string;
  title: string;
  readyState: string;
  text?: string;
  controls: { tag: string; id: string; type?: string; text: string }[];
}
export interface CorpusNativeTab {
  linkedBrowser: {
    browsingContext?: {
      currentWindowGlobal?: {
        isCurrentGlobal: boolean;
        documentURI?: { spec: string };
        getActor(
          name: string,
        ): { sendQuery(name: string): Promise<CorpusUISnapshot> };
      };
    };
  };
}
export interface CorpusBrowserWindow {
  gBrowser: {
    tabs: CorpusNativeTab[];
    addTab(
      uri: string,
      options: { triggeringPrincipal: unknown },
    ): CorpusNativeTab;
    removeTab(
      tab: CorpusNativeTab,
      options: { skipPermitUnload: boolean },
    ): void;
  };
}
export interface CorpusInstalledAddon {
  uninstall(): Promise<void>;
}
export interface CorpusInspectionReport {
  stage: string;
  primaryOperationPassRate: null;
  runtime: Record<string, unknown>;
  extensions: Record<string, unknown>[];
}
