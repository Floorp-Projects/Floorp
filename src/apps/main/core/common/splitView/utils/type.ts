/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { z } from "zod";

export const zSplitViewData = z.object({
  tabIds: z.array(z.string()),
  reverse: z.boolean(),
  method: z.enum(["row", "column"]),
  syncMode: z.boolean().optional(),
});

export const zSplitViewDatas = z.array(zSplitViewData);

export const zSyncDataGroup = z.object({
  sync: z.boolean(),
  options: z.object({
    reverse: z.boolean(),
    method: z.enum(["row", "column"]),
    syncMode: z.boolean(),
  }),
  syncTabId: z.string().nullable(),
});

export const zSplitViewConfigData = z.object({
  currentViewIndex: z.number(),
  syncData: zSyncDataGroup,
  splitViewData: zSplitViewDatas,
});

// Types for SplitView
export type TabEvent = {
  target: Tab;
  forUnsplit: boolean;
} & Event;

export type Browser = XULElement & {
  docShellIsActive: boolean;
  renderLayers: boolean;
  spliting: boolean;
};

export type Tab = XULElement & {
  linkedPanel: string;
  linkedBrowser: Browser;
  splitView: boolean;
};

export type SplitViewData = z.infer<typeof zSplitViewData>;
export type SyncDataGroup = z.infer<typeof zSyncDataGroup>;
export type SplitViewDatas = z.infer<typeof zSplitViewDatas>;
