// SPDX-License-Identifier: MPL-2.0
export type Adapter = {
  publishTokenFile(token: string): Promise<void>;
  cleanupTokenFile(): Promise<void>;
};
export type Options = {
  version?: number;
  missingInterface?: boolean;
  missingSecure?: boolean;
  failWrite?: boolean;
  removeFailures?: number;
  permanentRemoveFailure?: boolean;
};
