import { z } from "zod";

/**
 * ワークスペースの詳細情報
 */
export interface WorkspaceDetail {
  name: string;
  tabs: undefined[];
  defaultWorkspace: boolean;
  id: string;
  icon: string | null;
}

/**
 * 選択中ワークスペース情報
 */
export interface WorkspacePreference {
  selectedWorkspaceId: string;
}

/**
 * ウィンドウ内のワークスペースレコード
 */
export type WindowWorkspaces = Record<
  string,
  WorkspaceDetail | WorkspacePreference
>;

/**
 * PF11以前のワークスペース全体構造
 */
export interface Floorp11Workspaces {
  windows: Record<string, WindowWorkspaces>;
}

// Zod スキーマ定義
export const zWorkspaceDetail = z.object({
  name: z.string(),
  tabs: z.array(z.undefined()),
  defaultWorkspace: z.boolean(),
  id: z.string(),
  icon: z.string().nullable(),
});

export const zWorkspacePreference = z.object({
  selectedWorkspaceId: z.string(),
});

export const zWindowWorkspaces = z.record(
  z.string(),
  z.union([zWorkspaceDetail, zWorkspacePreference]),
);

export const zFloorp11WorkspacesSchema = z.object({
  windows: z.record(z.string(), zWindowWorkspaces),
});
