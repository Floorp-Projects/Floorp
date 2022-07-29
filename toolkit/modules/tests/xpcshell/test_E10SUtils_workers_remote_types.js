/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

const URI_SECURE_COM = Services.io.newURI("https://example.com");
const URI_SECURE_ORG = Services.io.newURI("https://example.org");
const URI_INSECURE_ORG = Services.io.newURI("http://example.org");
const URI_FILE = Services.io.newURI("file:///path/to/dir");
const URI_EXTENSION = Services.io.newURI("moz-extension://fake-uuid");
const URI_EXT_PROTOCOL = Services.io.newURI("ext+custom://fake-url");
const URI_WEB_PROTOCOL = Services.io.newURI("web+custom://fake-url");
const URI_PRIVILEGEDMOZILLA = Services.io.newURI("https://addons.mozilla.org");

const fakeContentScriptSandbox = Cu.Sandbox(
  ["https://example.org", "moz-extension://fake-uuid"],
  {}
);

const ssm = Services.scriptSecurityManager;
const systemPrincipal = ssm.getSystemPrincipal();
const nullPrincipal = ssm.createNullPrincipal({});
const principalSecureCom = ssm.createContentPrincipal(URI_SECURE_COM, {});
const principalSecureOrg = ssm.createContentPrincipal(URI_SECURE_ORG, {});
const principalInsecureOrg = ssm.createContentPrincipal(URI_INSECURE_ORG, {});
const principalFile = ssm.createContentPrincipal(URI_FILE, {});
const principalExtension = ssm.createContentPrincipal(URI_EXTENSION, {});
const principalExpanded = Cu.getObjectPrincipal(fakeContentScriptSandbox);
const principalExtProtocol = ssm.createContentPrincipal(URI_EXT_PROTOCOL, {});
const principalWebProtocol = ssm.createContentPrincipal(URI_WEB_PROTOCOL, {});
const principalPrivilegedMozilla = ssm.createContentPrincipal(
  URI_PRIVILEGEDMOZILLA,
  {}
);

const {
  EXTENSION_REMOTE_TYPE,
  FILE_REMOTE_TYPE,
  FISSION_WEB_REMOTE_TYPE,
  NOT_REMOTE,
  PRIVILEGEDABOUT_REMOTE_TYPE,
  PRIVILEGEDMOZILLA_REMOTE_TYPE,
  SERVICEWORKER_REMOTE_TYPE,
  WEB_REMOTE_COOP_COEP_TYPE_PREFIX,
  WEB_REMOTE_TYPE,
} = E10SUtils;

const {
  REMOTE_WORKER_TYPE_SHARED,
  REMOTE_WORKER_TYPE_SERVICE,
} = Ci.nsIE10SUtils;

// Test ServiceWorker remoteType selection with multiprocess and/or site
// isolation enabled.
add_task(function test_get_remote_type_for_service_worker() {
  // ServiceWorkers with system or null principal are unexpected and we expect
  // the method call to throw.
  for (const principal of [systemPrincipal, nullPrincipal]) {
    Assert.throws(
      () =>
        E10SUtils.getRemoteTypeForWorkerPrincipal(
          principal,
          REMOTE_WORKER_TYPE_SERVICE,
          true,
          false
        ),
      /Unexpected system or null principal/,
      `Did raise an exception on "${principal.origin}" principal ServiceWorker`
    );
  }

  // ServiceWorker test cases:
  // - e10s + fission disabled:
  //   - extension principal + any preferred remote type => extension remote type
  //   - content principal + any preferred remote type => web remote type
  // - fission enabled:
  //   - extension principal + any preferred remote type => extension remote type
  //   - content principal + any preferred remote type => webServiceWorker=siteOrigin remote type
  function* getTestCase(fission = false) {
    const TEST_PRINCIPALS = [
      principalSecureCom,
      principalSecureOrg,
      principalExtension,
    ];

    const PREFERRED_REMOTE_TYPES = [
      E10SUtils.DEFAULT_REMOTE_TYPE,
      E10SUtils.WEB_REMOTE_TYPE,
      "fakeRemoteType",
    ];

    for (const principal of TEST_PRINCIPALS) {
      for (const preferred of PREFERRED_REMOTE_TYPES) {
        const msg = `ServiceWorker, principal=${
          principal.origin
        }, preferredRemoteType=${preferred}, ${fission ? "fission" : "e10s"}`;
        yield [
          msg,
          principal,
          REMOTE_WORKER_TYPE_SERVICE,
          true,
          fission,
          preferred,
        ];
      }
    }
  }

  // Test cases for e10s mode + fission disabled.
  for (const testCase of getTestCase(false)) {
    const [msg, principal, ...args] = testCase;
    let expected = E10SUtils.WEB_REMOTE_TYPE;

    if (principal == principalExtension) {
      expected = WebExtensionPolicy.useRemoteWebExtensions
        ? E10SUtils.EXTENSION_REMOTE_TYPE
        : E10SUtils.NOT_REMOTE;
    }

    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(principal, ...args),
      expected,
      msg
    );
  }

  // Test cases for e10s mode + fission enabled.
  for (const testCase of getTestCase(true)) {
    const [msg, principal, ...args] = testCase;
    let expected = `${SERVICEWORKER_REMOTE_TYPE}=${principal.siteOrigin}`;

    if (principal == principalExtension) {
      expected = WebExtensionPolicy.useRemoteWebExtensions
        ? E10SUtils.EXTENSION_REMOTE_TYPE
        : E10SUtils.NOT_REMOTE;
    }

    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(principal, ...args),
      expected,
      msg
    );
  }
});

// Test SharedWorker remoteType selection with multiprocess and/or site
// isolation enabled.
add_task(function test_get_remote_type_for_shared_worker() {
  // Verify that for shared worker registered from a web coop+coep remote type
  // we are going to select a web or fission remote type.
  for (const [principal, preferredRemoteType] of [
    [
      principalSecureCom,
      `${WEB_REMOTE_COOP_COEP_TYPE_PREFIX}=${principalSecureCom.siteOrigin}`,
    ],
  ]) {
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principal,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        false,
        preferredRemoteType
      ),
      WEB_REMOTE_TYPE,
      `Got WEB_REMOTE_TYPE on preferred ${preferredRemoteType} and fission disabled`
    );

    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principal,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        preferredRemoteType
      ),
      `${FISSION_WEB_REMOTE_TYPE}=${principal.siteOrigin}`,
      `Got WEB_REMOTE_TYPE on preferred ${preferredRemoteType} and fission enabled`
    );
  }

  // For System principal shared worker we do select NOT_REMOTE or the preferred
  // remote type if is one of the explicitly allowed (NOT_REMOTE and
  // PRIVILEGEDABOUT_REMOTE_TYPE).
  for (const [principal, preferredRemoteType] of [
    [systemPrincipal, NOT_REMOTE],
    [systemPrincipal, PRIVILEGEDABOUT_REMOTE_TYPE],
  ]) {
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principal,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        preferredRemoteType
      ),
      preferredRemoteType,
      `Selected the preferred ${preferredRemoteType} on system principal shared worker`
    );
  }

  Assert.throws(
    () =>
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        systemPrincipal,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        "fakeRemoteType"
      ),
    /Failed to get a remoteType/,
    "Does fail explicitly on system worker for arbitrary preferredRemoteType"
  );

  // Behavior NOT_REMOTE preferredRemoteType for content principals with
  // multiprocess enabled.
  for (const [principal, expectedRemoteType] of [
    [
      principalSecureCom,
      `${FISSION_WEB_REMOTE_TYPE}=${principalSecureCom.siteOrigin}`,
    ],
    [
      principalExtension,
      WebExtensionPolicy.useRemoteWebExtensions
        ? EXTENSION_REMOTE_TYPE
        : NOT_REMOTE,
    ],
  ]) {
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principal,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        NOT_REMOTE
      ),
      expectedRemoteType,
      `Got ${expectedRemoteType} for content principal ${principal.siteOrigin}`
    );
  }

  // Shared worker registered for web+custom urls.
  for (const [preferredRemoteType, expectedRemoteType] of [
    [WEB_REMOTE_TYPE, WEB_REMOTE_TYPE],
    ["fakeRemoteType", "fakeRemoteType"],
    // This seems to be actually failing with a SecurityError
    // as soon as the SharedWorker constructor is being called with
    // a web+...:// url from an extension principal:
    //
    // [EXTENSION_REMOTE_TYPE, EXTENSION_REMOTE_TYPE],
  ]) {
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principalWebProtocol,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        preferredRemoteType
      ),
      expectedRemoteType,
      "Selected expected process for web+custom:// shared worker"
    );
  }

  // Shared worker registered for ext+custom urls.
  for (const [preferredRemoteType, expectedRemoteType] of [
    [WEB_REMOTE_TYPE, WEB_REMOTE_TYPE],
    ["fakeRemoteType", "fakeRemoteType"],
    // This seems to be actually prevented by failing a ClientIsValidPrincipalInfo
    // check (but only when the remote worker is being launched in the child process
    // and so after a remote Type has been selected).
    // [EXTENSION_REMOTE_TYPE, EXTENSION_REMOTE_TYPE],
  ]) {
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principalExtProtocol,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        preferredRemoteType
      ),
      expectedRemoteType,
      "Selected expected process for ext+custom:// shared worker"
    );
  }

  // Shared worker with a file principal.
  // NOTE: on android useSeparateFileUriProcess will be false and file uri are
  // going to run in the default web process.
  const expectedFileRemoteType = Services.prefs.getBoolPref(
    "browser.tabs.remote.separateFileUriProcess",
    false
  )
    ? FILE_REMOTE_TYPE
    : WEB_REMOTE_TYPE;

  for (const [preferredRemoteType, expectedRemoteType] of [
    [WEB_REMOTE_TYPE, expectedFileRemoteType],
    ["fakeRemoteType", expectedFileRemoteType],
  ]) {
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principalFile,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        preferredRemoteType
      ),
      expectedRemoteType,
      "Got expected remote type on file principal shared worker"
    );
  }

  // Shared worker related to a privilegedmozilla domain.
  // NOTE: separatePrivilegedMozilla will be false on android builds.
  const usePrivilegedMozilla = Services.prefs.getBoolPref(
    "browser.tabs.remote.separatePrivilegedMozillaWebContentProcess",
    false
  );
  const expectedRemoteType = usePrivilegedMozilla
    ? PRIVILEGEDMOZILLA_REMOTE_TYPE
    : `${FISSION_WEB_REMOTE_TYPE}=https://mozilla.org`;
  for (const preferredRemoteType of [
    PRIVILEGEDMOZILLA_REMOTE_TYPE,
    "fakeRemoteType",
  ]) {
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(
        principalPrivilegedMozilla,
        REMOTE_WORKER_TYPE_SHARED,
        true,
        true,
        preferredRemoteType
      ),
      expectedRemoteType,
      "Got expected remote type on privilegedmozilla principal shared worker"
    );
  }
});

// Test that we do throw on expanded principals.
add_task(function test_get_remote_type_throws_on_expanded_principals() {
  for (const workerType of [
    REMOTE_WORKER_TYPE_SHARED,
    REMOTE_WORKER_TYPE_SERVICE,
  ]) {
    Assert.throws(
      () =>
        E10SUtils.getRemoteTypeForWorkerPrincipal(
          principalExpanded,
          workerType,
          true,
          false
        ),
      /Unexpected expanded principal/,
      "Did raise an exception as expected"
    );
  }
});

// Test that NO_REMOTE is the remote type selected when multiprocess is disabled,
// there is no other checks special behaviors on particular principal or worker type.
add_task(function test_get_remote_type_multiprocess_disabled() {
  function* getTestCase() {
    const TEST_PRINCIPALS = [
      systemPrincipal,
      nullPrincipal,
      principalSecureCom,
      principalSecureOrg,
      principalInsecureOrg,
      principalFile,
      principalExtension,
    ];

    const PREFERRED_REMOTE_TYPES = [
      E10SUtils.DEFAULT_REMOTE_TYPE,
      E10SUtils.WEB_REMOTE_TYPE,
      "fakeRemoteType",
    ];

    for (const principal of TEST_PRINCIPALS) {
      for (const preferred of PREFERRED_REMOTE_TYPES) {
        const msg = `SharedWorker, principal=${principal.origin}, preferredRemoteType=${preferred}`;
        yield [
          msg,
          principal,
          REMOTE_WORKER_TYPE_SHARED,
          false,
          false,
          preferred,
        ];
      }
    }

    for (const principal of TEST_PRINCIPALS) {
      // system and null principals are disallowed for service workers, we throw
      // if passed to the E10SUtils method and we cover this scenario with a
      // separate test.
      if (principal.isSystemPrincipal || principal.isNullPrincipal) {
        continue;
      }
      for (const preferred of PREFERRED_REMOTE_TYPES) {
        const msg = `ServiceWorker with principal ${principal.origin} and preferredRemoteType ${preferred}`;
        yield [
          msg,
          principal,
          REMOTE_WORKER_TYPE_SERVICE,
          false,
          false,
          preferred,
        ];
      }
    }
  }

  for (const testCase of getTestCase()) {
    const [msg, ...args] = testCase;
    equal(
      E10SUtils.getRemoteTypeForWorkerPrincipal(...args),
      E10SUtils.NOT_REMOTE,
      `Expect NOT_REMOTE on disabled multiprocess: ${msg}`
    );
  }
});
