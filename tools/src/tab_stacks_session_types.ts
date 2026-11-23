// SPDX-License-Identifier: MPL-2.0

export type ProcessIdentity = {
  pid: number;
  parent: number;
  created: string;
  executable: string;
  command: string;
};

export type WindowState = {
  groups: { id: string; label: string; stacked: boolean; tabs: string[] }[];
  tabs: string[];
  bars: number;
  proxies: number;
  layout: {
    barHeight: number;
    barWidth: number;
    belowTabs: boolean;
    aboveNavigation: boolean;
  };
  selectedGroup: string | null;
  ready: boolean;
};
