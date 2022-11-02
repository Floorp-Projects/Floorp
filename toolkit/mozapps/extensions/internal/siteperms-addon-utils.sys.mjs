/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

export const GATED_PERMISSIONS = ["midi", "midi-sysex"];
export const SITEPERMS_ADDON_PROVIDER_PREF =
  "dom.sitepermsaddon-provider.enabled";
export const SITEPERMS_ADDON_TYPE = "sitepermission";
export const SITEPERMS_ADDON_BLOCKEDLIST_PREF =
  "dom.sitepermsaddon-provider.separatedBlocklistedDomains";

const lazy = {};
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "blocklistedOriginsSet",
  SITEPERMS_ADDON_BLOCKEDLIST_PREF,
  // Default value
  "",
  // onUpdate
  null,
  // transform
  prefValue => new Set(prefValue.split(","))
);

/**
 * @param {string} type
 * @returns {boolean}
 */
export function isGatedPermissionType(type) {
  return GATED_PERMISSIONS.includes(type);
}

/**
 * @param {string} siteOrigin
 * @returns {boolean}
 */
export function isKnownPublicSuffix(siteOrigin) {
  const { host } = new URL(siteOrigin);

  let isPublic = false;
  // getKnownPublicSuffixFromHost throws when passed an IP, in such case, assume
  // this is not a public etld.
  try {
    isPublic = Services.eTLD.getKnownPublicSuffixFromHost(host) == host;
  } catch (e) {}

  return isPublic;
}

/**
 * ⚠️ This should be only used for testing purpose ⚠️
 *
 * @param {Array<String>} permissionTypes
 * @throws if not called from xpcshell test
 */
export function addGatedPermissionTypesForXpcShellTests(permissionTypes) {
  if (!Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
    throw new Error("This should only be called from XPCShell tests");
  }

  GATED_PERMISSIONS.push(...permissionTypes);
}

/**
 * @param {nsIPrincipal} principal
 * @returns {Boolean}
 */
export function isPrincipalInSitePermissionsBlocklist(principal) {
  return lazy.blocklistedOriginsSet.has(principal.baseDomain);
}
