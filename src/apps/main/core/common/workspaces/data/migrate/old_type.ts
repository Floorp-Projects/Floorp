import { z } from "zod";

/**
 * ワークスペースの詳細情報
 */
export interface WorkspaceDetail {
  name: string;
  tabs: unknown[];
  defaultWorkspace: boolean;
  id: string;
  icon: string | null;
  userContextId?: number;
  isPrivateContainerWorkspace?: boolean;
}

/**
 * 選択中ワークスペース情報
 */
export interface WorkspacePreference {
  selectedWorkspaceId?: string;
  defaultWorkspace?: string;
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
  tabs: z.array(z.unknown()),
  defaultWorkspace: z.boolean(),
  id: z.string(),
  icon: z.string().nullable(),
  userContextId: z.number().optional(),
  isPrivateContainerWorkspace: z.boolean().optional(),
});

export const zWorkspacePreference = z.object({
  selectedWorkspaceId: z.string().optional(),
  defaultWorkspace: z.string().optional(),
});

export const zWindowWorkspaces = z.record(
  z.string(),
  z.union([zWorkspaceDetail, zWorkspacePreference]),
);

export const zFloorp11WorkspacesSchema = z.object({
  windows: z.record(z.string(), zWindowWorkspaces),
});
