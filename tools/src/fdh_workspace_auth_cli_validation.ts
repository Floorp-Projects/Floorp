// SPDX-License-Identifier: MPL-2.0
/** Same unprivileged literal-loopback contract at parser and runtime boundaries. */
export function workspaceProbeRedirect(raw: string): URL {
  const uri = new URL(raw);
  if (
    raw.length > 2048 || uri.href !== raw || uri.protocol !== "http:" ||
    uri.hostname !== "127.0.0.1" || uri.username || uri.password ||
    uri.pathname !== "/oauth/callback" || uri.search || uri.hash || !uri.port ||
    Number(uri.port) < 1024
  ) throw new Error("Workspace OAuth probe redirect rejected");
  return uri;
}
