// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertThrows } from "@std/assert";
import { parseSharedServiceConfig } from "./fdh_shared_config.ts";
const valid = () => ({
  publicOrigin: "https://fdh.example",
  issuer: "https://auth.example/realms/fdh",
  gatewayLoginUrl: "https://fdh.example/oauth2/start",
  expectedGoogleClientId: "123-example.apps.googleusercontent.com",
  readerClientId: "fdh-reader",
  readerSecretEnv: "FDH_BROKER_READER_SECRET",
});
Deno.test("absent shared settings stay unconfigured with no login or grant", () => {
  for (const input of [undefined, null]) {
    assertEquals(parseSharedServiceConfig(input), {
      mode: "unconfigured",
      loginAvailable: false,
      productionQualified: false,
      protectedOperationsAllowed: false,
    });
  }
});
Deno.test("configured public view never exposes server credential reference", () => {
  const result = parseSharedServiceConfig(valid());
  if (result.mode !== "configured") throw Error("Expected configured");
  assertEquals(result.public.loginPath, "/auth/login");
  assertEquals(result.public.mcpResource, "https://fdh.example/mcp");
  assertEquals(JSON.stringify(result.public).includes("SECRET"), false);
  assertEquals(JSON.stringify(result.public).includes("fdh-reader"), false);
  assertEquals(result.public.protectedOperationsAllowed, false);
  assertEquals(Object.isFrozen(result), true);
  assertEquals(Object.isFrozen(result.public), true);
  assertEquals(Object.isFrozen(result.server), true);
});
Deno.test("caller mutation cannot replace validated endpoints", () => {
  const input = valid();
  const result = parseSharedServiceConfig(input);
  input.publicOrigin = "https://attacker.example";
  if (result.mode !== "configured") throw Error("Expected configured");
  assertEquals(result.server.publicOrigin, "https://fdh.example");
});
for (const key of Object.keys(valid())) {
  Deno.test("missing required operator setting " + key + " rejects", () => {
    const input: Record<string, unknown> = valid();
    delete input[key];
    assertThrows(
      () => parseSharedServiceConfig(input),
      Error,
      "Shared service configuration rejected",
    );
  });
}
for (
  const url of [
    "http://fdh.example",
    "http://127.0.0.1:18881",
    "https://user:pass@fdh.example",
    "https://fdh.example/path",
    "https://fdh.example?x=1",
    "https://fdh.example#x",
    "https://FDH.example",
    " https://fdh.example",
    "https://fdh.example:443",
  ]
) {
  Deno.test(
    "noncanonical or insecure public origin rejects: " + url,
    () =>
      void assertThrows(
        () => parseSharedServiceConfig({ ...valid(), publicOrigin: url }),
        Error,
        "Shared service configuration rejected",
      ),
  );
}
for (
  const url of [
    "http://auth.example/realm",
    "https://user:pass@auth.example/realm",
    "https://auth.example/realm?x=1",
    "https://auth.example/realm#x",
    "https://auth.example/realm/",
  ]
) {
  Deno.test("invalid issuer rejects: " + url, () =>
    void assertThrows(
      () => parseSharedServiceConfig({ ...valid(), issuer: url }),
      Error,
      "Shared service configuration rejected",
    ));
}
for (
  const url of [
    "https://attacker.example/start",
    "http://fdh.example/start",
    "https://fdh.example/",
    "https://fdh.example/auth/login",
    "https://fdh.example/auth/bootstrap",
    "https://fdh.example/mcp",
    "https://fdh.example/api/auth/status",
    "https://fdh.example/oauth2/start?rd=https://attacker.example",
    "https://fdh.example/oauth2/start#x",
  ]
) {
  Deno.test(
    "gateway entry cannot redirect outside pinned origin or recurse: " + url,
    () =>
      void assertThrows(
        () => parseSharedServiceConfig({ ...valid(), gatewayLoginUrl: url }),
        Error,
        "Shared service configuration rejected",
      ),
  );
}
for (
  const key of [
    "googleClientSecret",
    "clientSecret",
    "token",
    "localSpike",
    "unknown",
  ]
) {
  Deno.test(
    "unknown or credential-valued config field rejected: " + key,
    () =>
      void assertThrows(
        () =>
          parseSharedServiceConfig({ ...valid(), [key]: "PRIVATE_FIXTURE" }),
        Error,
        "Shared service configuration rejected",
      ),
  );
}
Deno.test("getters are rejected without invocation", () => {
  let touched = false;
  const input = valid();
  Object.defineProperty(input, "issuer", {
    get() {
      touched = true;
      throw Error("PRIVATE_FIXTURE");
    },
  });
  assertThrows(
    () => parseSharedServiceConfig(input),
    Error,
    "Shared service configuration rejected",
  );
  assertEquals(touched, false);
});
Deno.test("proxy is rejected without traps", () => {
  let touched = false;
  const input = new Proxy(valid(), {
    ownKeys() {
      touched = true;
      throw Error("PRIVATE_FIXTURE");
    },
  });
  assertThrows(
    () => parseSharedServiceConfig(input),
    Error,
    "Shared service configuration rejected",
  );
  assertEquals(touched, false);
});
Deno.test("raw credential is never a credential-reference setting", () => {
  for (
    const readerSecretEnv of [
      "GOCSPX-private-value",
      "FDH_SECRET=private",
      "HOME",
      "PATH",
      "FDH_BROKER_READER_SECRET\n",
      "FDH_BROKER_READER_SECRET ",
    ]
  ) {
    assertThrows(
      () => parseSharedServiceConfig({ ...valid(), readerSecretEnv }),
      Error,
      "Shared service configuration rejected",
    );
  }
});
for (
  const input of [
    false,
    true,
    0,
    42,
    "",
    "PRIVATE_FIXTURE",
    [],
    new Date(0),
    Promise.resolve(null),
  ]
) {
  Deno.test("non-record configuration is rejected: " + typeof input, () => {
    const failure = assertThrows(() => parseSharedServiceConfig(input), Error);
    assertEquals(failure.message, "Shared service configuration rejected");
  });
}
Deno.test("null-prototype own configuration can be snapshotted", () => {
  const input = Object.assign(Object.create(null), valid());
  assertEquals(parseSharedServiceConfig(input).mode, "configured");
});
Deno.test("inherited configuration is not a trusted own snapshot", () => {
  assertThrows(() => parseSharedServiceConfig(Object.create(valid())), Error);
});
Deno.test("symbol property is rejected without calling it", () => {
  let touched = false;
  const input = valid();
  Object.defineProperty(input, Symbol.toStringTag, {
    get() {
      touched = true;
      throw Error("PRIVATE_FIXTURE");
    },
  });
  assertThrows(() => parseSharedServiceConfig(input), Error);
  assertEquals(touched, false);
});
for (const key of Object.keys(valid())) {
  Deno.test("accessor rejected without invocation for " + key, () => {
    let touched = false;
    const input = valid();
    Object.defineProperty(input, key, {
      get() {
        touched = true;
        throw Error("PRIVATE_FIXTURE");
      },
    });
    const failure = assertThrows(() => parseSharedServiceConfig(input), Error);
    assertEquals(failure.message, "Shared service configuration rejected");
    assertEquals(touched, false);
  });
  for (const suffix of ["\n", "\r", "\u0000"]) {
    Deno.test(
      "control character in " + key + " rejects " + JSON.stringify(suffix),
      () => {
        const failure = assertThrows(() =>
          parseSharedServiceConfig({
            ...valid(),
            [key]: "PRIVATE_FIXTURE" + suffix,
          }), Error);
        assertEquals(failure.message, "Shared service configuration rejected");
      },
    );
  }
}
for (
  const gatewayLoginUrl of [
    "https://fdh.example//oauth2/start",
    "https://fdh.example/oauth2/%73tart",
    "https://fdh.example/oauth2/start/../auth/login",
    "https://fdh.example/workspace",
  ]
) {
  Deno.test("ambiguous gateway route rejected: " + gatewayLoginUrl, () => {
    assertThrows(
      () => parseSharedServiceConfig({ ...valid(), gatewayLoginUrl }),
      Error,
    );
  });
}
Deno.test("oversize settings and invalid identifiers are never reflected", () => {
  for (const field of Object.keys(valid())) {
    const error = assertThrows(
      () =>
        parseSharedServiceConfig({
          ...valid(),
          [field]: "PRIVATE_FIXTURE".repeat(200),
        }),
      Error,
    );
    assertEquals(error.message, "Shared service configuration rejected");
  }
  for (
    const readerClientId of [
      "a b",
      "reader/client",
      "reader?token=PRIVATE_FIXTURE",
      "",
      "a".repeat(81),
    ]
  ) {
    assertThrows(
      () => parseSharedServiceConfig({ ...valid(), readerClientId }),
      Error,
    );
  }
  for (
    const expectedGoogleClientId of [
      "bad.example",
      "apps.googleusercontent.com",
      "PRIVATE_FIXTURE.apps.googleusercontent.com/",
      "x".repeat(201) + ".apps.googleusercontent.com",
    ]
  ) {
    assertThrows(
      () => parseSharedServiceConfig({ ...valid(), expectedGoogleClientId }),
      Error,
    );
  }
});
