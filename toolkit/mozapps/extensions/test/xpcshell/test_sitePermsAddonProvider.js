/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  addGatedPermissionTypesForXpcShellTests,
  SITEPERMS_ADDON_PROVIDER_PREF,
  SITEPERMS_ADDON_BLOCKEDLIST_PREF,
  SITEPERMS_ADDON_TYPE,
} = ChromeUtils.importESModule(
  "resource://gre/modules/addons/siteperms-addon-utils.sys.mjs"
);

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const addonsBundle = new Localization(["toolkit/about/aboutAddons.ftl"], true);

let ssm = Services.scriptSecurityManager;
const PRINCIPAL_COM = ssm.createContentPrincipalFromOrigin(
  "https://example.com"
);
const PRINCIPAL_ORG = ssm.createContentPrincipalFromOrigin(
  "https://example.org"
);
const PRINCIPAL_GITHUB =
  ssm.createContentPrincipalFromOrigin("https://github.io");
const PRINCIPAL_UNSECURE =
  ssm.createContentPrincipalFromOrigin("http://example.net");
const PRINCIPAL_IP = ssm.createContentPrincipalFromOrigin(
  "https://18.154.122.194"
);
const PRINCIPAL_PRIVATEBROWSING = ssm.createContentPrincipal(
  Services.io.newURI("https://example.withprivatebrowsing.com"),
  { privateBrowsingId: 1 }
);
const PRINCIPAL_USERCONTEXT = ssm.createContentPrincipal(
  Services.io.newURI("https://example.withusercontext.com"),
  { userContextId: 2 }
);
const PRINCIPAL_NULL = ssm.createNullPrincipal({});

const URI_USED_IN_MULTIPLE_CONTEXTS = Services.io.newURI(
  "https://multiplecontexts.com"
);
const PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR = ssm.createContentPrincipal(
  URI_USED_IN_MULTIPLE_CONTEXTS,
  {}
);
const PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING = ssm.createContentPrincipal(
  URI_USED_IN_MULTIPLE_CONTEXTS,
  { privateBrowsingId: 1 }
);
const PRINCIPAL_MULTIPLE_CONTEXTS_USERCONTEXT = ssm.createContentPrincipal(
  URI_USED_IN_MULTIPLE_CONTEXTS,
  { userContextId: 3 }
);

const BLOCKED_DOMAIN = "malicious.com";
const BLOCKED_DOMAIN2 = "someothermalicious.com";
const BLOCKED_PRINCIPAL = ssm.createContentPrincipalFromOrigin(
  `https://${BLOCKED_DOMAIN}`
);
const BLOCKED_PRINCIPAL2 = ssm.createContentPrincipalFromOrigin(
  `https://${BLOCKED_DOMAIN2}`
);

const GATED_SITE_PERM1 = "test/gatedSitePerm";
const GATED_SITE_PERM2 = "test/anotherGatedSitePerm";
addGatedPermissionTypesForXpcShellTests([GATED_SITE_PERM1, GATED_SITE_PERM2]);
const NON_GATED_SITE_PERM = "test/nonGatedPerm";

// Leave it to throw if the pref doesn't exist anymore, so that a test failure
// will make us notice and confirm if we have enabled it by default or not.
const PERMS_ISOLATE_USERCONTEXT_ENABLED = Services.prefs.getBoolPref(
  "permissions.isolateBy.userContext"
);

// Observers to bypass panels and continue install.
const expectAndHandleInstallPrompts = () => {
  TestUtils.topicObserved("addon-install-blocked").then(([subject]) => {
    let installInfo = subject.wrappedJSObject;
    info("==== test got addon-install-blocked, calling `install`");
    installInfo.install();
  });
  TestUtils.topicObserved("webextension-permission-prompt").then(
    ([subject]) => {
      info("==== test webextension-permission-prompt, calling `resolve`");
      subject.wrappedJSObject.info.resolve();
    }
  );
};

add_setup(async () => {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  AddonTestUtils.init(this);
  await promiseStartupManager();
});

add_task(
  {
    pref_set: [[SITEPERMS_ADDON_PROVIDER_PREF, false]],
  },
  async function test_sitepermsaddon_provider_disabled() {
    // The SitePermsAddonProvider does not register until the first content process
    // is launched, so we simulate that by firing this notification.
    Services.obs.notifyObservers(null, "ipc:first-content-process-created");

    ok(
      !AddonManager.hasProvider("SitePermsAddonProvider"),
      "Expect no SitePermsAddonProvider to be registered"
    );
  }
);

add_task(
  {
    pref_set: [
      [SITEPERMS_ADDON_PROVIDER_PREF, true],
      [
        SITEPERMS_ADDON_BLOCKEDLIST_PREF,
        `${BLOCKED_DOMAIN},${BLOCKED_DOMAIN2}`,
      ],
    ],
  },
  async function test_sitepermsaddon_provider_enabled() {
    // The SitePermsAddonProvider does not register until the first content process
    // is launched, so we simulate that by firing this notification.
    Services.obs.notifyObservers(null, "ipc:first-content-process-created");

    ok(
      AddonManager.hasProvider("SitePermsAddonProvider"),
      "Expect SitePermsAddonProvider to be registered"
    );

    let addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 0, "There's no addons");

    info("Add a gated permission");
    PermissionTestUtils.add(
      PRINCIPAL_COM,
      GATED_SITE_PERM1,
      Services.perms.ALLOW_ACTION
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 1, "A siteperm addon is now available");
    const comAddon = await promiseAddonByID(addons[0].id);
    Assert.equal(
      addons[0],
      comAddon,
      "getAddonByID returns the expected addon"
    );

    Assert.deepEqual(
      comAddon.sitePermissions,
      [GATED_SITE_PERM1],
      "addon has expected sitePermissions"
    );
    Assert.equal(
      comAddon.type,
      SITEPERMS_ADDON_TYPE,
      "addon has expected type"
    );

    const localizedExtensionName = await addonsBundle.formatValue(
      "addon-sitepermission-host",
      {
        host: PRINCIPAL_COM.host,
      }
    );
    Assert.ok(!!localizedExtensionName, "retrieved addonName is not falsy");
    Assert.equal(
      comAddon.name,
      localizedExtensionName,
      "addon has expected name"
    );

    info("Add another gated permission");
    PermissionTestUtils.add(
      PRINCIPAL_COM,
      GATED_SITE_PERM2,
      Services.perms.ALLOW_ACTION
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      1,
      "There is no new siteperm addon after adding a permission to the same principal..."
    );
    Assert.deepEqual(
      comAddon.sitePermissions,
      [GATED_SITE_PERM1, GATED_SITE_PERM2],
      "...but the new permission is reported by addon.sitePermissions"
    );

    info("Add a non-gated permission");
    PermissionTestUtils.add(
      PRINCIPAL_COM,
      NON_GATED_SITE_PERM,
      Services.perms.ALLOW_ACTION
    );
    Assert.equal(
      addons.length,
      1,
      "There is no new siteperm addon after adding a non gated permission to the same principal..."
    );
    Assert.deepEqual(
      comAddon.sitePermissions,
      [GATED_SITE_PERM1, GATED_SITE_PERM2],
      "...and the new permission is not reported by addon.sitePermissions"
    );

    info("Adding a gated permission to another principal");
    PermissionTestUtils.add(
      PRINCIPAL_ORG,
      GATED_SITE_PERM1,
      Services.perms.ALLOW_ACTION
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 2, "A new siteperm addon is now available");
    const orgAddon = await promiseAddonByID(addons[1].id);
    Assert.equal(
      addons[1],
      orgAddon,
      "getAddonByID returns the expected addon"
    );

    Assert.deepEqual(
      orgAddon.sitePermissions,
      [GATED_SITE_PERM1],
      "new addon only has a single sitePermission"
    );

    info(
      "Passing null or undefined to getAddonsByTypes returns all the addons"
    );
    addons = await promiseAddonsByTypes(null);
    // We can't do an exact check on all the returned addons as we get other type
    // of addons from other providers.
    Assert.deepEqual(
      addons.filter(a => a.type == SITEPERMS_ADDON_TYPE).map(a => a.id),
      [comAddon.id, orgAddon.id],
      "Got site perms addons when passing null"
    );

    addons = await promiseAddonsByTypes();
    Assert.deepEqual(
      addons.filter(a => a.type == SITEPERMS_ADDON_TYPE).map(a => a.id),
      [comAddon.id, orgAddon.id],
      "Got site perms addons when passing undefined"
    );

    info("Remove a gated permission");
    PermissionTestUtils.remove(PRINCIPAL_COM, GATED_SITE_PERM2);
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "Removing a permission did not removed the addon has it has another permission"
    );
    Assert.deepEqual(
      comAddon.sitePermissions,
      [GATED_SITE_PERM1],
      "addon has expected sitePermissions"
    );

    info("Remove last gated permission on PRINCIPAL_COM");
    const promisePrincipalComUninstalling = AddonTestUtils.promiseAddonEvent(
      "onUninstalling",
      addon => {
        return addon.id === comAddon.id;
      }
    );
    const promisePrincipalComUninstalled = AddonTestUtils.promiseAddonEvent(
      "onUninstalled",
      addon => {
        return addon.id === comAddon.id;
      }
    );
    PermissionTestUtils.remove(PRINCIPAL_COM, GATED_SITE_PERM1);
    info("Wait for onUninstalling addon listener call");
    await promisePrincipalComUninstalling;
    info("Wait for onUninstalled addon listener call");
    await promisePrincipalComUninstalled;

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      1,
      "Removing the last gated permission removed the addon"
    );
    Assert.equal(addons[0], orgAddon);

    info("Uninstall org addon");
    orgAddon.uninstall();
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 0, "org addon is removed");
    Assert.equal(
      PermissionTestUtils.testExactPermission(PRINCIPAL_ORG, GATED_SITE_PERM1),
      false,
      "Permission was removed when the addon was uninstalled"
    );

    info("Add gated permissions");
    PermissionTestUtils.add(
      PRINCIPAL_COM,
      GATED_SITE_PERM1,
      Services.perms.ALLOW_ACTION
    );
    PermissionTestUtils.add(
      PRINCIPAL_ORG,
      GATED_SITE_PERM1,
      Services.perms.ALLOW_ACTION
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 2, "2 addons are now available");

    info("Clear permissions");
    const onAddon1Uninstall = AddonTestUtils.promiseAddonEvent(
      "onUninstalled",
      addon => addon.id === addons[0].id
    );
    const onAddon2Uninstall = AddonTestUtils.promiseAddonEvent(
      "onUninstalled",
      addon => addon.id === addons[1].id
    );
    Services.perms.removeAll();

    await Promise.all([onAddon1Uninstall, onAddon2Uninstall]);
    ok("Addons were properly uninstalled…");
    Assert.equal(
      (await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE])).length,
      0,
      "… and getAddonsByTypes does not return them anymore"
    );

    info("Adding a permission to a public etld");
    PermissionTestUtils.add(
      PRINCIPAL_GITHUB,
      GATED_SITE_PERM1,
      Services.perms.ALLOW_ACTION
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      0,
      "Adding a gated permission to a public etld shouldn't add a new addon"
    );
    // Cleanup
    PermissionTestUtils.remove(PRINCIPAL_GITHUB, GATED_SITE_PERM1);

    info("Adding a permission to a non secure principal");
    PermissionTestUtils.add(
      PRINCIPAL_UNSECURE,
      GATED_SITE_PERM1,
      Services.perms.ALLOW_ACTION
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      0,
      "Adding a gated permission to an unsecure principal shouldn't add a new addon"
    );

    info("Adding a permission to a blocked principal");
    PermissionTestUtils.add(
      BLOCKED_PRINCIPAL,
      GATED_SITE_PERM1,
      Services.perms.ALLOW_ACTION
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      0,
      "Adding a gated permission to a blocked principal shouldn't add a new addon"
    );
    // Cleanup
    PermissionTestUtils.remove(BLOCKED_PRINCIPAL, GATED_SITE_PERM1);

    info("Call installSitePermsAddonFromWebpage without proper principal");
    await Assert.rejects(
      AddonManager.installSitePermsAddonFromWebpage(
        null,
        // principal
        null,
        GATED_SITE_PERM1
      ),
      /aInstallingPrincipal must be a nsIPrincipal/,
      "installSitePermsAddonFromWebpage rejected when called without a principal"
    );

    info(
      "Call installSitePermsAddonFromWebpage with non-null, non-element browser"
    );
    await Assert.rejects(
      AddonManager.installSitePermsAddonFromWebpage(
        "browser",
        PRINCIPAL_COM,
        GATED_SITE_PERM1
      ),
      /aBrowser must be an Element, or null/,
      "installSitePermsAddonFromWebpage rejected when called  with a non-null, non-element browser"
    );

    info("Call installSitePermsAddonFromWebpage with unsecure principal");
    await Assert.rejects(
      AddonManager.installSitePermsAddonFromWebpage(
        null,
        PRINCIPAL_UNSECURE,
        GATED_SITE_PERM1
      ),
      /SitePermsAddons can only be installed from secure origins/,
      "installSitePermsAddonFromWebpage rejected when called with unsecure principal"
    );

    info(
      "Call installSitePermsAddonFromWebpage for public principal and gated permission"
    );
    await Assert.rejects(
      AddonManager.installSitePermsAddonFromWebpage(
        null,
        PRINCIPAL_GITHUB,
        GATED_SITE_PERM1
      ),
      /SitePermsAddon can\'t be installed from public eTLDs/,
      "installSitePermsAddonFromWebpage rejected when called with public principal"
    );

    info(
      "Call installSitePermsAddonFromWebpage for a NullPrincipal as installingPrincipal"
    );
    await Assert.rejects(
      AddonManager.installSitePermsAddonFromWebpage(
        null,
        PRINCIPAL_NULL,
        GATED_SITE_PERM1
      ),
      /SitePermsAddons can\'t be installed from sandboxed subframes/,
      "installSitePermsAddonFromWebpage rejected when called with a NullPrincipal as installing principal"
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 0, "there was no added addon...");
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_GITHUB,
        GATED_SITE_PERM1
      ),
      false,
      "...and no new permission either"
    );

    info(
      "Call installSitePermsAddonFromWebpage for plain-ip principal and gated permission"
    );
    await Assert.rejects(
      AddonManager.installSitePermsAddonFromWebpage(
        null,
        PRINCIPAL_IP,
        GATED_SITE_PERM1
      ),
      /SitePermsAddons install disallowed when the host is an IP address/,
      "installSitePermsAddonFromWebpage rejected when called with plain-ip principal"
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 0, "there was no added addon...");
    Assert.equal(
      PermissionTestUtils.testExactPermission(PRINCIPAL_IP, GATED_SITE_PERM1),
      false,
      "...and no new permission either"
    );

    info(
      "Call installSitePermsAddonFromWebpage for authorized principal and non-gated permission"
    );
    await Assert.rejects(
      AddonManager.installSitePermsAddonFromWebpage(
        null,
        PRINCIPAL_COM,
        NON_GATED_SITE_PERM
      ),
      new RegExp(`"${NON_GATED_SITE_PERM}" is not a gated permission`),
      "installSitePermsAddonFromWebpage rejected for non-gated permission"
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      0,
      "installSitePermsAddonFromWebpage with non-gated permission should not add the addon"
    );

    info("Call installSitePermsAddonFromWebpage blocked principals");
    const blockedDomainsPrincipals = {
      [BLOCKED_DOMAIN]: BLOCKED_PRINCIPAL,
      [BLOCKED_DOMAIN2]: BLOCKED_PRINCIPAL2,
    };
    for (const blockedDomain of [BLOCKED_DOMAIN, BLOCKED_DOMAIN2]) {
      await Assert.rejects(
        AddonManager.installSitePermsAddonFromWebpage(
          null,
          blockedDomainsPrincipals[blockedDomain],
          GATED_SITE_PERM1
        ),
        /SitePermsAddons can\'t be installed/,
        `installSitePermsAddonFromWebpage rejected when called with blocked domain principal: ${blockedDomainsPrincipals[blockedDomain].URI.spec}`
      );
      await Assert.rejects(
        AddonManager.installSitePermsAddonFromWebpage(
          null,
          ssm.createContentPrincipalFromOrigin(`https://sub.${blockedDomain}`),
          GATED_SITE_PERM1
        ),
        /SitePermsAddons can\'t be installed/,
        `installSitePermsAddonFromWebpage rejected when called with blocked subdomain principal: https://sub.${blockedDomain}`
      );
    }

    info(
      "Call installSitePermsAddonFromWebpage for authorized principal and gated permission"
    );
    expectAndHandleInstallPrompts();
    const onAddonInstalled = AddonTestUtils.promiseInstallEvent(
      "onInstallEnded"
    ).then(installs => installs?.[0]?.addon);

    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_COM,
      GATED_SITE_PERM1
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      1,
      "installSitePermsAddonFromWebpage should add the addon..."
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(PRINCIPAL_COM, GATED_SITE_PERM1),
      true,
      "...and set the permission"
    );

    // The addon we get here is a SitePermsAddonInstalling instance, and we want to assert
    // that its permissions are correct as it may impact addon uninstall later on
    Assert.deepEqual(
      (await onAddonInstalled).sitePermissions,
      [GATED_SITE_PERM1],
      "Addon has expected sitePermissions"
    );

    info(
      "Call installSitePermsAddonFromWebpage for private browsing principal and gated permission"
    );
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_PRIVATEBROWSING,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      true,
      "...and set the permission"
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "installSitePermsAddonFromWebpage should add the addon..."
    );
    const [addonWithPrivateBrowsing] = addons.filter(
      addon => addon.siteOrigin === PRINCIPAL_PRIVATEBROWSING.siteOriginNoSuffix
    );
    Assert.equal(
      addonWithPrivateBrowsing?.siteOrigin,
      PRINCIPAL_PRIVATEBROWSING.siteOriginNoSuffix,
      "Got an addon for the expected siteOriginNoSuffix value"
    );
    await addonWithPrivateBrowsing.uninstall();
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      false,
      "Uninstalling the addon should clear the permission for the private browsing principal"
    );

    info(
      "Call installSitePermsAddonFromWebpage for user context isolated principal and gated permission"
    );
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_USERCONTEXT,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_USERCONTEXT,
        GATED_SITE_PERM1
      ),
      true,
      "...and set the permission"
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "installSitePermsAddonFromWebpage should add the addon..."
    );
    const [addonWithUserContextId] = addons.filter(
      addon => addon.siteOrigin === PRINCIPAL_USERCONTEXT.siteOriginNoSuffix
    );
    Assert.equal(
      addonWithUserContextId?.siteOrigin,
      PRINCIPAL_USERCONTEXT.siteOriginNoSuffix,
      "Got an addon for the expected siteOriginNoSuffix value"
    );
    await addonWithUserContextId.uninstall();
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_USERCONTEXT,
        GATED_SITE_PERM1
      ),
      false,
      "Uninstalling the addon should clear the permission for the user context isolated principal"
    );

    info(
      "Check calling installSitePermsAddonFromWebpage for same gated permission and same origin on different contexts"
    );
    info("First call installSitePermsAddonFromWebpage on regular context");
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
        GATED_SITE_PERM1
      ),
      true,
      "...and set the permission"
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_USERCONTEXT,
        GATED_SITE_PERM1
      ),
      // We expect the permission to not be set for user context if
      // perms.isolateBy.userContext is set to true.
      PERMS_ISOLATE_USERCONTEXT_ENABLED
        ? Services.perms.UNKNOWN_ACTION
        : Services.perms.ALLOW_ACTION,
      `...and ${
        PERMS_ISOLATE_USERCONTEXT_ENABLED ? "not allowed" : "allowed"
      } on the context user specific principal`
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      false,
      "...but not on the private browsing principal"
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "installSitePermsAddonFromWebpage should add the addon..."
    );
    const [addonMultipleContexts] = addons.filter(
      addon =>
        addon.siteOrigin ===
        PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR.siteOriginNoSuffix
    );
    Assert.equal(
      addonMultipleContexts?.siteOrigin,
      PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR.siteOriginNoSuffix,
      "Got an addon for the expected siteOriginNoSuffix value"
    );
    info("Then call installSitePermsAddonFromWebpage on private context");
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      true,
      "Permission is set for the private context"
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
        GATED_SITE_PERM1
      ),
      true,
      "...and still set on the regular principal"
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_USERCONTEXT,
        GATED_SITE_PERM1
      ),
      // We expect the permission to not be set for user context if
      // perms.isolateBy.userContext is set to true.
      PERMS_ISOLATE_USERCONTEXT_ENABLED
        ? Services.perms.UNKNOWN_ACTION
        : Services.perms.ALLOW_ACTION,
      `...and ${
        PERMS_ISOLATE_USERCONTEXT_ENABLED ? "not allowed" : "allowed"
      } on the context user specific principal`
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "installSitePermsAddonFromWebpage did not add a new addon"
    );
    info("Then call installSitePermsAddonFromWebpage on specific user context");
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_MULTIPLE_CONTEXTS_USERCONTEXT,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_USERCONTEXT,
        GATED_SITE_PERM1
      ),
      true,
      "Permission is set for the user context"
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
        GATED_SITE_PERM1
      ),
      true,
      "...and still set on the regular principal"
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      true,
      "...and on the private principal"
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "installSitePermsAddonFromWebpage did not add a new addon"
    );

    info(
      "Uninstalling the addon should remove the permission on the different contexts"
    );
    await addonMultipleContexts.uninstall();
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
        GATED_SITE_PERM1
      ),
      false,
      "Uninstalling the addon should clear the permission for regular principal..."
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      false,
      "... as well as the private browsing one..."
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_USERCONTEXT,
        GATED_SITE_PERM1
      ),
      false,
      "... and the user context one"
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 1, "addon was properly uninstalled");

    info("Install the addon for the multiple context origin again");
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
        GATED_SITE_PERM1
      ),
      true,
      "...and set the permission"
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "installSitePermsAddonFromWebpage should add the addon..."
    );

    info("Then call installSitePermsAddonFromWebpage on private context");
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      true,
      "Permission is set for the private context"
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      2,
      "installSitePermsAddonFromWebpage did not add a new addon"
    );

    info("Remove the permission for the private context");
    PermissionTestUtils.remove(
      PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_PRIVATEBROWSING,
        GATED_SITE_PERM1
      ),
      false,
      "Permission is removed for the private context..."
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 2, "... but it didn't uninstall the addon");

    info("Remove the permission for the regular context");
    PermissionTestUtils.remove(
      PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
      GATED_SITE_PERM1
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        PRINCIPAL_MULTIPLE_CONTEXTS_REGULAR,
        GATED_SITE_PERM1
      ),
      false,
      "Permission is removed for the regular context..."
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 1, "...and addon is uninstalled");

    await addons[0]?.uninstall();
  }
);

add_task(
  {
    pref_set: [[SITEPERMS_ADDON_PROVIDER_PREF, true]],
  },
  async function test_salted_hash_addon_id() {
    // Make sure the test will also be able to run if it is the only one executed.
    Services.obs.notifyObservers(null, "ipc:first-content-process-created");
    ok(
      AddonManager.hasProvider("SitePermsAddonProvider"),
      "Expect SitePermsAddonProvider to be registered"
    );
    // Make sure no sitepermission addon is already installed.
    let addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 0, "There's no addons");

    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_COM,
      GATED_SITE_PERM1
    );
    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(
      addons.length,
      1,
      "installSitePermsAddonFromWebpage should add the addon..."
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(PRINCIPAL_COM, GATED_SITE_PERM1),
      true,
      "...and set the permission"
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 1, "There is an addon installed");

    const firstSaltedAddonId = addons[0].id;
    ok(firstSaltedAddonId, "Got the first addon id");

    info("Verify addon id after mocking new browsing session");

    const { generateSalt } = ChromeUtils.importESModule(
      "resource://gre/modules/addons/SitePermsAddonProvider.sys.mjs"
    );
    generateSalt();

    await promiseRestartManager();

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 1, "There is an addon installed");

    const secondSaltedAddonId = addons[0].id;
    ok(
      secondSaltedAddonId,
      "Got the second addon id after mocking new browsing session"
    );

    Assert.notEqual(
      firstSaltedAddonId,
      secondSaltedAddonId,
      "The two addon ids are different"
    );

    // Confirm that new installs from the same siteOrigin will still
    // belong to the existing addon entry while the salt isn't expected
    // to have changed.
    expectAndHandleInstallPrompts();
    await AddonManager.installSitePermsAddonFromWebpage(
      null,
      PRINCIPAL_COM,
      GATED_SITE_PERM1
    );

    addons = await promiseAddonsByTypes([SITEPERMS_ADDON_TYPE]);
    Assert.equal(addons.length, 1, "There is still a single addon installed");

    await addons[0]?.uninstall();
  }
);
