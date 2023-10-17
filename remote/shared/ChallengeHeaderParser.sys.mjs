/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Parse the parameter in a name/value pair and remove quotes.
 *
 * @param {string} paramValue
 *     A string representing a challenge parameter.
 *
 * @returns {object}
 *     An object with name and value string properties.
 */
function parseChallengeParameter(paramValue) {
  const [name, value] = paramValue.split("=");
  return { name, value: value?.replace(/["']/g, "") };
}

/**
 * Simple parser for authenticate (WWW-Authenticate or Proxy-Authenticate)
 * headers.
 *
 * Bug 1857847: Replace with Necko's ChallengeParser once exposed to JS.
 *
 * @param {string} headerValue
 *     The value of an authenticate header.
 *
 * @returns {Array<object>}
 *     Array of challenge objects containing two properties:
 *     - {string} scheme: The scheme for the challenge
 *     - {Array<object>} params: Array of { name, value } objects representing
 *       all the parameters of the challenge.
 */
export function parseChallengeHeader(headerValue) {
  const challenges = [];
  const parts = headerValue.split(",").map(part => part.trim());

  let scheme = null;
  let params = [];

  const schemeRegex = /^(\w+)(?:\s+(.*))?$/;
  for (const part of parts) {
    const matches = part.match(schemeRegex);
    if (matches !== null) {
      // This is a new scheme.
      if (scheme !== null) {
        // If we have a challenge recorded, add it to the array.
        challenges.push({ scheme, params });
      }

      // Reset the state for a new scheme.
      scheme = matches[1];
      params = [];
      if (matches[2]) {
        params.push(parseChallengeParameter(matches[2]));
      }
    } else {
      if (scheme === null) {
        // A scheme should always be found before parameters, this header
        // probably needs a more careful parsing solution.
        return [];
      }

      params.push(parseChallengeParameter(part));
    }
  }

  if (scheme !== null) {
    // If we have a challenge recorded, add it to the array.
    challenges.push({ scheme, params });
  }

  return challenges;
}
