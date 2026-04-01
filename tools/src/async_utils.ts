// SPDX-License-Identifier: MPL-2.0

export function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
