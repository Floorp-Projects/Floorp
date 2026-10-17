// SPDX-License-Identifier: MPL-2.0

export interface DevServerOptions {
  testSourceBinding?: boolean;
}

export type DevServerWriter =
  | { write?: (message: string) => unknown; end?: () => unknown }
  | ((message: string) => unknown)
  | null;
