// SPDX-License-Identifier: MPL-2.0

/**
 * Shared utilities for OS test wrapper scripts.
 */

export function hasFlag(args: string[], flag: string): boolean {
  return args.includes(flag) || args.some((arg) => arg.startsWith(`${flag}=`));
}

/**
 * Build an argument array by appending default flags that are not already
 * present in `argv`.
 */
export function withDefaultFlags(argv: string[], defaults: string[]): string[] {
  const nextArgs = [...argv];
  for (const flag of defaults) {
    if (!hasFlag(nextArgs, flag)) {
      nextArgs.push(flag);
    }
  }
  return nextArgs;
}
