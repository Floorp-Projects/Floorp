// SPDX-License-Identifier: MPL-2.0

/**
 * Global `feles-build` arguments parsed before command dispatch.
 *
 * `--artifact <path>` is a *global* option even though only the `dev`
 * command consumes it. The canonical form places it before the command:
 * `feles-build --artifact <path> dev`. Both `--artifact <path>` and
 * `--artifact=<path>` are accepted and removed from `argv` so the command
 * dispatcher never sees them.
 */

export interface FelesBuildGlobalArgs {
  artifactPath?: string;
  argv: string[];
}

const ARTIFACT_FLAG = "--artifact";
const ARTIFACT_FLAG_PREFIX = `${ARTIFACT_FLAG}=`;

function readArtifactValue(
  argv: readonly string[],
  index: number,
  inlineValue: string | undefined,
): { value: string; consumed: number } {
  if (inlineValue !== undefined) {
    if (inlineValue.trim().length === 0) {
      throw new Error("--artifact requires a non-empty path value.");
    }
    return { value: inlineValue, consumed: 1 };
  }
  const value = argv[index + 1];
  if (
    value === undefined ||
    value.trim().length === 0 ||
    value.startsWith("-")
  ) {
    throw new Error("--artifact requires a non-empty path value.");
  }
  return { value, consumed: 2 };
}

export function parseFelesBuildGlobalArgs(
  argv: readonly string[],
): FelesBuildGlobalArgs {
  const remaining: string[] = [];
  let artifactPath: string | undefined;

  for (let index = 0; index < argv.length; index += 1) {
    const token = argv[index];
    const inlineValue = token === ARTIFACT_FLAG
      ? undefined
      : token.startsWith(ARTIFACT_FLAG_PREFIX)
      ? token.slice(ARTIFACT_FLAG_PREFIX.length)
      : null;

    if (token !== ARTIFACT_FLAG && inlineValue === null) {
      remaining.push(token);
      continue;
    }
    if (artifactPath !== undefined) {
      throw new Error("--artifact may be specified only once.");
    }
    const { value, consumed } = readArtifactValue(
      argv,
      index,
      inlineValue ?? undefined,
    );
    artifactPath = value;
    index += consumed - 1;
  }

  return { artifactPath, argv: remaining };
}
