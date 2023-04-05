/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function isExtensionContext(browsingContext) {
  let principal;
  if (CanonicalBrowsingContext.isInstance(browsingContext)) {
    principal = browsingContext.currentWindowGlobal.documentPrincipal;
  } else {
    principal = browsingContext.window.document.nodePrincipal;
  }

  // In practice, note that the principal will never be an expanded principal.
  // The are only used for content scripts executed in a Sandbox, and do not
  // have a browsing context on their own.
  // But we still use this flag because there is no isAddonPrincipal flag.
  return principal.isAddonOrExpandedAddonPrincipal;
}

function isParentProcess(browsingContext) {
  if (CanonicalBrowsingContext.isInstance(browsingContext)) {
    return browsingContext.currentWindowGlobal.osPid === -1;
  }

  // If `browsingContext` is not a `CanonicalBrowsingContext`, then we are
  // necessarily in a content process page.
  return false;
}

/**
 * Check if the given browsing context is valid for the message handler
 * to use.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context to check.
 * @param {object=} options
 * @param {string=} options.browserId
 *    The id of the browser to filter the browsing contexts by (optional).
 * @returns {boolean}
 *     True if the browsing context is valid, false otherwise.
 */
export function isBrowsingContextCompatible(browsingContext, options = {}) {
  const { browserId } = options;

  // If a browserId was provided, skip browsing contexts which are not
  // associated with this browserId.
  if (browserId !== undefined && browsingContext.browserId !== browserId) {
    return false;
  }

  // Skip:
  // - extension contexts until we support debugging webextensions, see Bug 1755014.
  // - privileged contexts until we support debugging Chrome context, see Bug 1713440.
  return (
    !isExtensionContext(browsingContext) && !isParentProcess(browsingContext)
  );
}
