// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertThrows } from "@std/assert";
import { createSharedGoogleBrokerResolver } from "./fdh_shared_google_broker.ts";
import type {
  GoogleBrokerFixture,
  SharedGoogleBrokerTrustedIO,
} from "./fdh_shared_google_broker_types.ts";

const policy = {
  issuer: "https://as.example.test/realms/floorp",
  resourceAudience: "https://fdh.example.test/mcp",
  requiredScopes: ["fdh:probe"],
  expectedGoogleClientId: "public-fixture.apps.googleusercontent.com",
};
const access = {
  kind: "verified-shared-access-token",
  issuer: policy.issuer,
  subject: "realm-user",
  resourceAudience: policy.resourceAudience,
  expiresAt: 2000,
  scopes: ["openid", "fdh:probe"],
};
function fixture(): GoogleBrokerFixture {
  return {
    introspection: {
      active: true,
      iss: policy.issuer,
      sub: access.subject,
      aud: [policy.resourceAudience],
      scope: "openid fdh:probe",
      exp: 2000,
      identity_provider: "google",
    },
    user: {
      id: access.subject,
      email: "fixture@floorp.app",
      emailVerified: true,
    },
    identities: [{ identityProvider: "google", userId: "opaque-google-sub" }],
    provider: {
      alias: "google",
      providerId: "google",
      enabled: true,
      trustEmail: true,
      storeToken: false,
      config: {
        syncMode: "FORCE",
        validateSignature: "true",
        disableUserInfo: "true",
        filteredByClaim: "true",
        claimFilterName: "email_verified",
        claimFilterValue: "true",
        disableNonce: "false",
        hostedDomain: "floorp.app",
        clientId: policy.expectedGoogleClientId,
      },
    },
    mappers: [],
    profile: {
      unmanagedAttributePolicy: "DISABLED",
      attributes: [
        {
          name: "email",
          permissions: { view: ["admin", "user"], edit: ["admin"] },
        },
        { name: "username", permissions: { edit: ["admin"] } },
      ],
    },
  };
}
function io(f: GoogleBrokerFixture): SharedGoogleBrokerTrustedIO {
  return {
    introspect: () => Promise.resolve(f.introspection),
    readUser: () => Promise.resolve(f.user),
    readFederatedIdentities: () => Promise.resolve(f.identities),
    readGoogleProviderConfig: () => Promise.resolve(f.provider),
    readGoogleProviderMappers: () => Promise.resolve(f.mappers),
    readUserProfile: () => Promise.resolve(f.profile),
    now: () => 1000,
  };
}
Deno.test("Google broker synthetic fixture keeps Google sub separate; not actual Google proof or MCP grant", async () => {
  const binding = await createSharedGoogleBrokerResolver(policy, io(fixture()))(
    "synthetic-token",
    access,
  );
  assertEquals(binding, {
    kind: "verified-online-google-binding",
    active: true,
    sessionProvider: "google",
    authorizationSubject: "realm-user",
    identityKey: "opaque-google-sub",
    identityIssuer: "https://accounts.google.com",
    email: "fixture@floorp.app",
    emailVerified: true,
    hostedDomain: "floorp.app",
  });
});
for (
  const [key, bad] of Object.entries({
    active: [false, "true", 1, null],
    iss: ["wrong"],
    sub: ["other", null],
    aud: ["client-id", [], ["wrong"], [policy.resourceAudience, 1]],
    scope: ["fdh:probe", "openid", "openid  fdh:probe", []],
    exp: [1000, 1999, "2000", 2000.1],
    identity_provider: ["local", "github", null],
    grant_type: ["client_credentials"],
  })
) {
  for (const value of bad) {
    Deno.test(
      "denies introspection " + key + "=" + JSON.stringify(value),
      async () => {
        const f = fixture();
        f.introspection[key] = value;
        assertEquals(
          await createSharedGoogleBrokerResolver(policy, io(f))(
            "synthetic-token",
            access,
          ),
          null,
        );
      },
    );
  }
}
for (
  const key of [
    "active",
    "iss",
    "sub",
    "aud",
    "scope",
    "exp",
    "identity_provider",
  ]
) {
  Deno.test("denies missing introspection " + key, async () => {
    const f = fixture();
    delete f.introspection[key];
    assertEquals(
      await createSharedGoogleBrokerResolver(policy, io(f))(
        "synthetic-token",
        access,
      ),
      null,
    );
  });
}
for (
  const [key, bad] of Object.entries({
    alias: ["local", null],
    providerId: ["oidc", null],
    enabled: [false, "true"],
    trustEmail: [false, "true"],
    storeToken: [true, "false"],
    linkOnly: [true],
  })
) {
  for (const value of bad) {
    Deno.test(
      "denies provider " + key + "=" + JSON.stringify(value),
      async () => {
        const f = fixture();
        f.provider[key] = value;
        assertEquals(
          await createSharedGoogleBrokerResolver(policy, io(f))(
            "synthetic-token",
            access,
          ),
          null,
        );
      },
    );
  }
}
for (
  const [key, bad] of Object.entries({
    syncMode: ["IMPORT", "LEGACY"],
    validateSignature: ["false", true],
    disableUserInfo: ["false", true],
    filteredByClaim: ["false", true],
    claimFilterName: ["email", "hd"],
    claimFilterValue: [".*", "false", "true|false", true],
    disableNonce: ["true", false],
    hostedDomain: ["floorp.app.evil", "*"],
    clientId: ["other-public-client"],
    userIDClaim: ["email"],
    emailClaim: ["preferred_username"],
  })
) {
  for (const value of bad) {
    Deno.test(
      "denies config " + key + "=" + JSON.stringify(value),
      async () => {
        const f = fixture();
        (f.provider.config as Record<string, unknown>)[key] = value;
        assertEquals(
          await createSharedGoogleBrokerResolver(policy, io(f))(
            "synthetic-token",
            access,
          ),
          null,
        );
      },
    );
  }
}
for (
  const key of [
    "syncMode",
    "validateSignature",
    "disableUserInfo",
    "filteredByClaim",
    "claimFilterName",
    "claimFilterValue",
    "hostedDomain",
    "clientId",
  ]
) {
  Deno.test("denies missing config " + key, async () => {
    const f = fixture();
    delete (f.provider.config as Record<string, unknown>)[key];
    assertEquals(
      await createSharedGoogleBrokerResolver(policy, io(f))(
        "synthetic-token",
        access,
      ),
      null,
    );
  });
}
Deno.test("official absent nonce default, audience string and scope subset work", async () => {
  const f = fixture();
  delete (f.provider.config as Record<string, unknown>).disableNonce;
  f.introspection.aud = policy.resourceAudience;
  f.introspection.scope = "openid fdh:probe extra:scope";
  assertEquals(
    (await createSharedGoogleBrokerResolver(policy, io(f))(
      "synthetic-token",
      access,
    ))?.identityKey,
    "opaque-google-sub",
  );
});
for (
  const [key, bad] of Object.entries({
    id: ["other"],
    emailVerified: [false, "true", 1],
    email: [
      "fixture@other.app",
      "fixture@floorp.app.evil",
      "fixture@@floorp.app",
      " fixture@floorp.app",
      "fixture@floorp.app\n",
      "fixture@FLOORP.APP",
      ".fixture@floorp.app",
      "fixture..x@floorp.app",
    ],
    serviceAccountClientId: ["machine"],
    enabled: [false],
  })
) {
  for (const value of bad) {
    Deno.test("denies user " + key + "=" + JSON.stringify(value), async () => {
      const f = fixture();
      f.user[key] = value;
      assertEquals(
        await createSharedGoogleBrokerResolver(policy, io(f))(
          "synthetic-token",
          access,
        ),
        null,
      );
    });
  }
}
for (
  const identities of [
    null,
    {},
    [],
    [{ identityProvider: "local", userId: "spoof" }],
    [{ identityProvider: "google", userId: "" }],
    [{ identityProvider: "google", userId: "x\n" }],
    [{ identityProvider: "google" }],
    [{ identityProvider: "google", userId: 123 }],
    [{ identityProvider: "google", userId: "one" }, {
      identityProvider: "google",
      userId: "two",
    }],
  ]
) {
  Deno.test(
    "denies missing or ambiguous federation " + JSON.stringify(identities),
    async () => {
      const f = fixture();
      f.identities = identities;
      f.user.attributes = { googleSub: ["forged"] };
      assertEquals(
        await createSharedGoogleBrokerResolver(policy, io(f))(
          "synthetic-token",
          access,
        ),
        null,
      );
    },
  );
}
Deno.test("never substitutes editable user attributes or realm sub for Google identity", async () => {
  const f = fixture();
  f.user.attributes = { googleSub: ["forged"], hd: ["floorp.app"] };
  assertEquals(
    (await createSharedGoogleBrokerResolver(policy, io(f))(
      "synthetic-token",
      access,
    ))?.identityKey,
    "opaque-google-sub",
  );
});
for (
  const mappers of [null, {}, [{
    identityProviderMapper: "oidc-user-attribute-idp-mapper",
  }], [null]]
) {
  Deno.test(
    "denies any mapper or incomplete mapper list " + JSON.stringify(mappers),
    async () => {
      const f = fixture();
      f.mappers = mappers;
      assertEquals(
        await createSharedGoogleBrokerResolver(policy, io(f))(
          "synthetic-token",
          access,
        ),
        null,
      );
    },
  );
}
for (
  const profile of [
    null,
    {},
    { unmanagedAttributePolicy: "ENABLED", attributes: [] },
    { unmanagedAttributePolicy: "DISABLED", attributes: [] },
    ...[undefined, [], ["user"], ["admin", "user"]].map((edit) => ({
      unmanagedAttributePolicy: "DISABLED",
      attributes: [
        { name: "email", permissions: { ...(edit ? { edit } : {}) } },
        { name: "username", permissions: { edit: ["admin"] } },
      ],
    })),
    {
      unmanagedAttributePolicy: "DISABLED",
      attributes: [
        { name: "email", permissions: { edit: ["admin"] } },
        { name: "email", permissions: { edit: ["admin"] } },
        { name: "username", permissions: { edit: ["admin"] } },
      ],
    },
  ]
) {
  Deno.test(
    "denies unlocked/incomplete UserProfileConfig " + JSON.stringify(profile),
    async () => {
      const deps = {
        ...io(fixture()),
        readUserProfile: () => Promise.resolve(profile),
      };
      assertEquals(
        await createSharedGoogleBrokerResolver(policy, deps)(
          "synthetic-token",
          access,
        ),
        null,
      );
    },
  );
}
for (const key of Object.keys(access)) {
  Deno.test("denies missing verified access " + key, async () => {
    const a: Record<string, unknown> = { ...access };
    delete a[key];
    assertEquals(
      await createSharedGoogleBrokerResolver(policy, io(fixture()))(
        "synthetic-token",
        a,
      ),
      null,
    );
  });
}
for (const value of [null, [], {}, "claims", 1, new Proxy(access, {})]) {
  Deno.test(
    "rejects unknown access shape " + String(typeof value),
    async () => {
      assertEquals(
        await createSharedGoogleBrokerResolver(policy, io(fixture()))(
          "synthetic-token",
          value,
        ),
        null,
      );
    },
  );
}
for (const value of [null, {}, "", "x\n", "x".repeat(16385)]) {
  Deno.test("rejects invalid token shape " + String(typeof value), async () => {
    let calls = 0;
    const deps = {
      ...io(fixture()),
      introspect: () => {
        calls++;
        return Promise.resolve(null);
      },
    };
    assertEquals(
      await createSharedGoogleBrokerResolver(policy, deps)(value, access),
      null,
    );
    assertEquals(calls, 0);
  });
}
for (
  const name of [
    "introspect",
    "readUser",
    "readFederatedIdentities",
    "readGoogleProviderConfig",
    "readGoogleProviderMappers",
    "readUserProfile",
  ] as const
) {
  Deno.test(
    "does not expose secret-bearing IO error from " + name,
    async () => {
      const deps = {
        ...io(fixture()),
        [name]: () => {
          throw Error("UNIT_SECRET_DO_NOT_ECHO");
        },
      };
      assertEquals(
        await createSharedGoogleBrokerResolver(policy, deps)(
          "synthetic-token",
          access,
        ),
        null,
      );
    },
  );
}
Deno.test("construction snapshots policy and callback references", async () => {
  const p = { ...policy, requiredScopes: [...policy.requiredScopes] };
  const mutable = { ...io(fixture()) };
  const resolve = createSharedGoogleBrokerResolver(p, mutable);
  p.issuer = "other";
  p.requiredScopes[0] = "other";
  mutable.introspect = () => Promise.resolve(null);
  mutable.now = () => 5000;
  assertEquals(
    (await resolve("synthetic-token", access))?.identityKey,
    "opaque-google-sub",
  );
});
Deno.test("access snapshot detached before first await", async () => {
  const f = fixture();
  const a = { ...access, scopes: [...access.scopes] };
  const deps = {
    ...io(f),
    introspect: () => {
      a.subject = "forged";
      a.expiresAt = 0;
      a.scopes[0] = "forged";
      return Promise.resolve(f.introspection);
    },
  };
  assertEquals(
    (await createSharedGoogleBrokerResolver(policy, deps)("synthetic-token", a))
      ?.authorizationSubject,
    "realm-user",
  );
});
Deno.test("response snapshot detached before subsequent awaits", async () => {
  const f = fixture();
  const deps = {
    ...io(f),
    readFederatedIdentities: () => {
      f.user.email = "attacker@evil.test";
      f.introspection.sub = "forged";
      return Promise.resolve(f.identities);
    },
  };
  assertEquals(
    (await createSharedGoogleBrokerResolver(policy, deps)(
      "synthetic-token",
      access,
    ))?.email,
    "fixture@floorp.app",
  );
});
for (const end of [2000, 2001, 999, NaN]) {
  Deno.test("denies expiry or clock rollback across IO " + end, async () => {
    let calls = 0;
    const deps = {
      ...io(fixture()),
      now: () => ++calls === 1 ? 1000 : end,
    };
    assertEquals(
      await createSharedGoogleBrokerResolver(policy, deps)(
        "synthetic-token",
        access,
      ),
      null,
    );
  });
}
Deno.test("rejects config accessors and proxies without invoking them", async () => {
  const f = fixture();
  let traps = 0;
  f.provider.config = new Proxy({}, {
    get: () => {
      traps++;
      throw Error();
    },
    ownKeys: () => {
      traps++;
      throw Error();
    },
    getPrototypeOf: () => {
      traps++;
      throw Error();
    },
  });
  assertEquals(
    await createSharedGoogleBrokerResolver(policy, io(f))(
      "synthetic-token",
      access,
    ),
    null,
  );
  assertEquals(traps, 0);
  const g = fixture();
  Object.defineProperty(g.provider, "config", {
    get: () => {
      traps++;
      throw Error();
    },
  });
  assertEquals(
    await createSharedGoogleBrokerResolver(policy, io(g))(
      "synthetic-token",
      access,
    ),
    null,
  );
  assertEquals(traps, 0);
});
Deno.test("rejects inherited fields, cycles, oversized and accessor arrays", async () => {
  for (
    const bad of [
      Object.create({ active: true }),
      new Date(),
      new Map(),
      new Array(257),
      new Proxy([], {}),
    ]
  ) {
    const deps = { ...io(fixture()), introspect: () => Promise.resolve(bad) };
    assertEquals(
      await createSharedGoogleBrokerResolver(policy, deps)(
        "synthetic-token",
        access,
      ),
      null,
    );
  }
  const f = fixture();
  f.introspection.cycle = f.introspection;
  assertEquals(
    await createSharedGoogleBrokerResolver(policy, io(f))(
      "synthetic-token",
      access,
    ),
    null,
  );
  const g = fixture();
  let calls = 0;
  const arr = ["google"];
  Object.defineProperty(arr, "0", {
    get: () => {
      calls++;
      return "google";
    },
  });
  g.identities = arr;
  assertEquals(
    await createSharedGoogleBrokerResolver(policy, io(g))(
      "synthetic-token",
      access,
    ),
    null,
  );
  assertEquals(calls, 0);
});
Deno.test("opaque Google sub is not restricted to numeric or realm ID syntax", async () => {
  const f = fixture();
  f.identities = [{
    identityProvider: "google",
    userId: "opaque:google.subject/123",
  }];
  assertEquals(
    (await createSharedGoogleBrokerResolver(policy, io(f))(
      "synthetic-token",
      access,
    ))?.identityKey,
    "opaque:google.subject/123",
  );
});
Deno.test("token limit is 16KiB rather than the policy text limit", async () => {
  assertEquals(
    (await createSharedGoogleBrokerResolver(policy, io(fixture()))(
      "x".repeat(4096),
      access,
    ))?.identityKey,
    "opaque-google-sub",
  );
});
Deno.test("invalid factory input has constant nonsecret diagnostics", () => {
  for (
    const bad of [
      null,
      {},
      [],
      { ...policy, requiredScopes: ["bad scope"] },
      new Proxy(policy, {}),
    ]
  ) {
    const error = assertThrows(
      () => createSharedGoogleBrokerResolver(bad, io(fixture())),
      Error,
    );
    assertEquals(error.message, "Invalid Google broker configuration");
  }
  let calls = 0;
  const deps = { ...io(fixture()) };
  Object.defineProperty(deps, "introspect", {
    get: () => {
      calls++;
      throw Error("UNIT_SECRET");
    },
  });
  assertThrows(() => createSharedGoogleBrokerResolver(policy, deps));
  assertEquals(calls, 0);
});
