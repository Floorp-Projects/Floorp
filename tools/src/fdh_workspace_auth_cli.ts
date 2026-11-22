// SPDX-License-Identifier: MPL-2.0
import { workspaceProbeRedirect } from "./fdh_workspace_auth_cli_validation.ts";
import type {
  WorkspaceAuthCliDependencies,
  WorkspaceAuthCliResult,
  WorkspaceAuthProbeOptions,
} from "./fdh_workspace_auth_cli_types.ts";

const HELP =
  "fdh auth probe --client-id <public-client-id> --redirect-uri http://127.0.0.1:<unprivileged-port>/oauth/callback [--client-secret-env NAME] [--timeout-ms 300000]\nOperator-initiated Google Workspace OAuth code/PKCE + refresh + logout probe. Uses memory only; no tokens are printed or persisted. A browser consent step is required; no browser is launched automatically. Exact redirect must be registered with Google. Provider credentials may only come from an explicitly named environment variable, never an argv value. Does not issue MCP/runner credentials or qualify AC-05.";
const FLAGS = new Set([
  "client-id",
  "redirect-uri",
  "client-secret-env",
  "timeout-ms",
]);
const FAIL = "Workspace OAuth probe rejected";
/** Raw argv parser: no duplicate-collapsing generic parser or I/O for help/errors. */
export async function runWorkspaceAuthCli(
  argv: readonly string[],
  dependencies: WorkspaceAuthCliDependencies = {},
): Promise<WorkspaceAuthCliResult> {
  try {
    if (argv[0] !== "auth") throw new Error();
    if (
      (argv.length === 1) || (argv.length === 2 && argv[1] === "--help") ||
      (argv.length === 3 && argv[1] === "probe" && argv[2] === "--help")
    ) return { exitCode: 0, stdout: HELP };
    if (argv[1] !== "probe") throw new Error();
    const flags = new Map<string, string>();
    for (let i = 2; i < argv.length; i += 2) {
      const name = argv[i]?.slice(2);
      const value = argv[i + 1];
      if (
        !argv[i]?.startsWith("--") || !FLAGS.has(name) || flags.has(name) ||
        value === undefined || value.startsWith("--") || value.length === 0
      ) throw new Error();
      flags.set(name, value);
    }
    const clientId = flags.get("client-id");
    const redirectUri = flags.get("redirect-uri");
    const timeout = flags.get("timeout-ms") ?? "300000";
    const secretEnv = flags.get("client-secret-env");
    if (
      clientId === undefined || clientId.length > 256 ||
      [...clientId].some((character) => {
        const code = character.charCodeAt(0);
        return code <= 32 || code === 127;
      }) || redirectUri === undefined ||
      !/^[1-9][0-9]{0,5}$/.test(timeout) || Number(timeout) > 900000 ||
      (secretEnv !== undefined && !/^[A-Z_][A-Z0-9_]{0,127}$/.test(secretEnv))
    ) throw new Error();
    workspaceProbeRedirect(redirectUri);
    // Only an explicitly named variable is ever read, after complete arg validation.
    const clientSecret = secretEnv === undefined
      ? undefined
      : (dependencies.readEnv ?? Deno.env.get)(secretEnv);
    if (
      secretEnv !== undefined &&
      (typeof clientSecret !== "string" || !clientSecret ||
        clientSecret.length > 4096 ||
        [...clientSecret].some((character) => {
          const code = character.charCodeAt(0);
          return code < 32 || code === 127;
        }))
    ) throw new Error();
    const options: WorkspaceAuthProbeOptions = Object.freeze({
      clientId,
      clientSecret,
      redirectUri,
      timeoutMs: Number(timeout),
    });
    const probe = dependencies.probe ??
      (await import("./fdh_workspace_auth_cli_runtime.ts"))
        .runWorkspaceAuthProbe;
    const result = await probe(options);
    // Do not serialize arbitrary adapter properties (or provider token material).
    const output = {
      schemaVersion: 1,
      scope: "workspace-oauth-probe",
      codeExchangeVerified: result.codeExchangeVerified,
      refreshVerified: result.refreshVerified,
      localLogout: result.localLogout,
      providerRevocationConfirmed: result.providerRevocationConfirmed,
      identity: {
        subject: result.identity.subject,
        email: result.identity.email,
        hostedDomain: result.identity.hostedDomain,
        issuer: result.identity.issuer,
      },
      mcpAuthorizationGranted: false,
      ac05Qualified: false,
    };
    return {
      exitCode: output.providerRevocationConfirmed === true ? 0 : 1,
      stdout: JSON.stringify(output),
    };
  } catch {
    return {
      exitCode: 1,
      stdout: JSON.stringify({
        error: FAIL,
        mcpAuthorizationGranted: false,
        ac05Qualified: false,
      }),
    };
  }
}
