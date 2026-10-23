// SPDX-License-Identifier: MPL-2.0

export interface FileOutputProcess {
  pid: number;
  status: Promise<{ code: number }>;
  kill(signal?: Deno.Signal): void;
}
