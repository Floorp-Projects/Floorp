// SPDX-License-Identifier: MPL-2.0

/**
 * Pure option resolution for the OS-Server verification wrapper
 * (tools/os-test/run_verify_os_server_full_wrapper.ts, KAN-135).
 *
 * Base URL precedence, explicit sources first:
 *   1. --base-url CLI flag;
 *   2. FLOORP_OS_BASE_URL environment variable;
 *   3. derived default, which depends on the start mode:
 *      - --skip-start without --port-offset: an already-running target
 *        (typically an installed Floorp) keeps the packaged production
 *        default port;
 *      - otherwise the wrapper starts a dev browser, whose profile always
 *        runs the OS-Server on the dev-only in-span port
 *        (DEV_OS_SERVER_PORT + offset, 5175 by default).
 *
 * A --port-offset together with --skip-start still targets a dev launch
 * (the running dev instance with that offset), so the derived port follows
 * the offset instead of falling back to production.
 */

import type {
  OsVerifyOptions,
  OsVerifyRawOptions,
} from "./os_test_options_types.ts";
import { resolveOsTestBaseUrl } from "./os_test_client.ts";
import {
  devOsServerBaseUrl,
  parsePortOffsetSpec,
  type PortOffsetSpec,
} from "./port_offset.ts";

/**
 * Parse the wrapper's --port-offset argument (KAN-135 CLI edges).
 *
 * Pure and synchronous: no port probing and no browser start happen here.
 * - absent flag (undefined): resolved to undefined (no offset given);
 * - a bare flag with an empty value: rejected, so a missing value cannot
 *   silently default to offset 0;
 * - "auto" together with --skip-start: rejected before any probing - an
 *   auto offset resolves a FREE launch topology, which by definition has no
 *   running OS-Server to target, and the offset of an already-running
 *   instance cannot be inferred;
 * - "auto" without --skip-start: returned as "auto" for the caller to
 *   resolve with resolveAutoDevPortOffset();
 * - otherwise the validated numeric offset (shared validation error text).
 */
export function parseWrapperPortOffsetArg(
  raw: string | undefined,
  skipStart: boolean,
): PortOffsetSpec | undefined {
  if (raw === undefined) {
    return undefined;
  }
  if (raw.trim().length === 0) {
    throw new Error("--port-offset requires a value <n|auto>");
  }
  const spec = parsePortOffsetSpec(raw);
  if (spec === "auto" && skipStart) {
    throw new Error(
      "--port-offset auto cannot be combined with --skip-start: auto resolves a free launch topology, not the offset of an already-running instance (pass the concrete offset)",
    );
  }
  return spec;
}

/**
 * Resolve the wrapper options.
 *
 * The offset is validated up front through the shared dev-port-offset
 * error even when an explicit base URL bypasses the derived port, so an
 * invalid offset fails identically everywhere. Throws when portOffset is
 * not an admissible dev port offset.
 */
export function resolveOsVerifyOptions(
  raw: OsVerifyRawOptions,
): OsVerifyOptions {
  // An explicit --port-offset always signals a dev launch target, even
  // together with --skip-start.
  const targetsDevLaunch = !raw.skipStart || raw.portOffset !== undefined;

  if (targetsDevLaunch) {
    const baseUrl = resolveOsTestBaseUrl(
      raw.baseUrlFlag,
      raw.envBaseUrl,
      devOsServerBaseUrl(raw.portOffset ?? 0),
    );
    return {
      baseUrl,
      // Only the auto-start path actually launches: skip-start targets an
      // already-running instance, so no start command arguments are added.
      startCommandArgs: raw.skipStart || raw.portOffset === undefined
        ? []
        : ["--port-offset", String(raw.portOffset)],
      portOffset: raw.portOffset ?? 0,
    };
  }

  // skip-start without an offset: existing already-running target, keep the
  // packaged production default for installed-Floorp compatibility.
  return {
    baseUrl: resolveOsTestBaseUrl(raw.baseUrlFlag, raw.envBaseUrl),
    startCommandArgs: [],
    portOffset: raw.portOffset ?? 0,
  };
}
