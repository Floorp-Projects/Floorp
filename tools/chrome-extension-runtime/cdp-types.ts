// SPDX-License-Identifier: MPL-2.0
import type { ProtocolMapping } from "devtools-protocol/types/protocol-mapping.d.ts";

export type CDPMethod = keyof ProtocolMapping.Commands;
export type CDPResult<Method extends CDPMethod> = Method extends
  "Runtime.evaluate"
  ? Omit<ProtocolMapping.Commands[Method]["returnType"], "result"> & {
    result:
      & Omit<ProtocolMapping.Commands[Method]["returnType"]["result"], "value">
      & {
        value?: unknown;
      };
  }
  : ProtocolMapping.Commands[Method]["returnType"];

export interface CDPEvent {
  method: string;
  params?: Record<string, unknown>;
  sessionId?: string;
}

export interface CDPMessage extends Partial<CDPEvent> {
  id?: number;
  result?: unknown;
  error?: unknown;
}

export interface PendingCDPCall {
  resolve(value: unknown): void;
  reject(error: Error): void;
}
