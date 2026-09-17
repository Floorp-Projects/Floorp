// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  assessG5DesktopTwoClientLifecycleEvidence,
} from "./g5_desktop_two_client_lifecycle_contract.ts";
import {
  createOfflineFakeG5DesktopTwoClientExecutor,
  G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY,
} from "./g5_desktop_two_client_executor.ts";

const RUN_ID = "g5-run-20260814-003";
const EXECUTOR_INSTANCE_ID = "g5-executor-20260814-003";
const PAIR_ID = "g5-pair-20260814-003";
const FIXTURE_SCHEMA = "floorp-g5-desktop-two-client-offline-fixture-v2";
const DYNAMIC_IMPORT_TOKEN =
  /\bimport\b(?:(?:\s|\/\*[\s\S]*?\*\/|\/\/[^\n]*)*)\(/u;
const TYPE_ONLY_NAMED_SPECIFIER =
  /^type\s+[A-Za-z_$][\w$]*(?:\s+as\s+[A-Za-z_$][\w$]*)?$/u;

interface ModuleImportScan {
  readonly dynamicImport: boolean;
  readonly runtimeSources: readonly string[];
}

interface ParsedFromModule {
  readonly clause: string;
  readonly next: number;
  readonly source: string | undefined;
}

interface ParsedStringLiteral {
  readonly next: number;
  readonly value: string;
}

interface MutableClientFixture {
  captureProofId: string;
  clientInstanceId: string;
  fictional: boolean;
  port: number;
  profileToken: string;
  rootPid: number;
  rootProcessGeneration: string;
  terminationProofId: string;
}

interface MutableFixture {
  clients: [MutableClientFixture, MutableClientFixture];
  executorInstanceId: string;
  mode: string;
  pairId: string;
  runId: string;
  schemaVersion: string;
}

function client(
  clientInstanceId: string,
  rootPid: number,
  port: number,
  profileToken: string,
  captureProofId: string,
  terminationProofId: string,
): MutableClientFixture {
  return {
    captureProofId,
    clientInstanceId,
    fictional: true,
    port,
    profileToken,
    rootPid,
    rootProcessGeneration: `pid-${rootPid}-generation-987654321`,
    terminationProofId,
  };
}

function fixture(): MutableFixture {
  return {
    clients: [
      client(
        "g5-client-a",
        4_201,
        28_291,
        "g5-profile-token-a",
        "g5-capture-proof-a",
        "g5-termination-proof-a",
      ),
      client(
        "g5-client-b",
        4_202,
        28_292,
        "g5-profile-token-b",
        "g5-capture-proof-b",
        "g5-termination-proof-b",
      ),
    ],
    executorInstanceId: EXECUTOR_INSTANCE_ID,
    mode: G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY,
    pairId: PAIR_ID,
    runId: RUN_ID,
    schemaVersion: FIXTURE_SCHEMA,
  };
}

function fixtureJson(value: MutableFixture = fixture()): string {
  return JSON.stringify(value);
}

function legacyClientKeyOrderJson(value: MutableFixture = fixture()): string {
  return JSON.stringify({
    clients: value.clients.map((client) => ({
      captureProofId: client.captureProofId,
      clientInstanceId: client.clientInstanceId,
      fictional: client.fictional,
      profileToken: client.profileToken,
      port: client.port,
      rootPid: client.rootPid,
      rootProcessGeneration: client.rootProcessGeneration,
      terminationProofId: client.terminationProofId,
    })),
    executorInstanceId: value.executorInstanceId,
    mode: value.mode,
    pairId: value.pairId,
    runId: value.runId,
    schemaVersion: value.schemaVersion,
  });
}

function isTypeOnlyModuleClause(clause: string): boolean {
  const normalized = clause.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/gu, " ")
    .replace(/\s+/gu, " ").trim();
  if (normalized.startsWith("type ")) return true;
  if (!normalized.startsWith("{") || !normalized.endsWith("}")) {
    return false;
  }
  const namedSpecifiers = normalized.slice(1, -1).split(",").map((specifier) =>
    specifier.trim()
  ).filter((specifier) => specifier.length > 0);
  return namedSpecifiers.length > 0 &&
    namedSpecifiers.every((specifier) =>
      TYPE_ONLY_NAMED_SPECIFIER.test(specifier)
    );
}

function isIdentifierPart(value: string | undefined): boolean {
  return value !== undefined && /[A-Za-z0-9_$]/u.test(value);
}

function hasKeywordAt(source: string, index: number, keyword: string): boolean {
  return source.startsWith(keyword, index) &&
    !isIdentifierPart(source[index - 1]) &&
    !isIdentifierPart(source[index + keyword.length]);
}

function skipTrivia(source: string, index: number): number {
  let cursor = index;
  while (cursor < source.length) {
    if (/\s/u.test(source[cursor])) {
      cursor += 1;
      continue;
    }
    if (source.startsWith("//", cursor)) {
      const newline = source.indexOf("\n", cursor + 2);
      cursor = newline === -1 ? source.length : newline + 1;
      continue;
    }
    if (source.startsWith("/*", cursor)) {
      const end = source.indexOf("*/", cursor + 2);
      cursor = end === -1 ? source.length : end + 2;
      continue;
    }
    return cursor;
  }
  return cursor;
}

function parseStringLiteral(
  source: string,
  index: number,
): ParsedStringLiteral | undefined {
  const quote = source[index];
  if (quote !== '"' && quote !== "'") return undefined;
  let cursor = index + 1;
  while (cursor < source.length) {
    if (source[cursor] === "\\") {
      cursor += 2;
      continue;
    }
    if (source[cursor] === quote) {
      return {
        next: cursor + 1,
        value: source.slice(index + 1, cursor),
      };
    }
    cursor += 1;
  }
  return undefined;
}

function skipTemplateLiteral(source: string, index: number): number {
  let cursor = index + 1;
  while (cursor < source.length) {
    if (source[cursor] === "\\") {
      cursor += 2;
      continue;
    }
    if (source[cursor] === "`") return cursor + 1;
    cursor += 1;
  }
  return source.length;
}

function parseFromModule(source: string, index: number): ParsedFromModule {
  let braceDepth = 0;
  let cursor = index;
  while (cursor < source.length) {
    const next = skipTrivia(source, cursor);
    if (next !== cursor) {
      cursor = next;
      continue;
    }
    if (source[cursor] === "{") {
      braceDepth += 1;
      cursor += 1;
      continue;
    }
    if (source[cursor] === "}") {
      braceDepth = Math.max(0, braceDepth - 1);
      cursor += 1;
      continue;
    }
    if (source[cursor] === ";") {
      return {
        clause: source.slice(index, cursor),
        next: cursor + 1,
        source: undefined,
      };
    }
    if (braceDepth === 0 && hasKeywordAt(source, cursor, "from")) {
      const moduleStart = skipTrivia(source, cursor + "from".length);
      const module = parseStringLiteral(source, moduleStart);
      return {
        clause: source.slice(index, cursor),
        next: module?.next ?? moduleStart + 1,
        source: module?.value,
      };
    }
    if (source[cursor] === '"' || source[cursor] === "'") {
      const literal = parseStringLiteral(source, cursor);
      return {
        clause: source.slice(index, cursor),
        next: literal?.next ?? cursor + 1,
        source: undefined,
      };
    }
    if (source[cursor] === "`") {
      return {
        clause: source.slice(index, cursor),
        next: skipTemplateLiteral(source, cursor),
        source: undefined,
      };
    }
    cursor += 1;
  }
  return {
    clause: source.slice(index),
    next: source.length,
    source: undefined,
  };
}

function scanModuleImports(source: string): ModuleImportScan {
  let dynamicImport = false;
  const runtimeSources: string[] = [];
  let cursor = 0;

  while (cursor < source.length) {
    const next = skipTrivia(source, cursor);
    if (next !== cursor) {
      cursor = next;
      continue;
    }
    if (source[cursor] === '"' || source[cursor] === "'") {
      cursor = parseStringLiteral(source, cursor)?.next ?? cursor + 1;
      continue;
    }
    if (source[cursor] === "`") {
      cursor = skipTemplateLiteral(source, cursor);
      continue;
    }
    if (source[cursor - 1] === ".") {
      cursor += 1;
      continue;
    }
    if (hasKeywordAt(source, cursor, "import")) {
      const declarationStart = skipTrivia(source, cursor + "import".length);
      if (source[declarationStart] === "(") {
        dynamicImport = true;
        cursor = declarationStart + 1;
        continue;
      }
      const sideEffect = parseStringLiteral(source, declarationStart);
      if (sideEffect !== undefined) {
        runtimeSources.push(sideEffect.value);
        cursor = sideEffect.next;
        continue;
      }
      if (source[declarationStart] === ".") {
        cursor = declarationStart + 1;
        continue;
      }
      const parsed = parseFromModule(source, declarationStart);
      if (parsed.source === undefined) {
        runtimeSources.push("<unparseable-runtime-import>");
      } else if (
        !hasKeywordAt(source, declarationStart, "type") &&
        !isTypeOnlyModuleClause(parsed.clause)
      ) {
        runtimeSources.push(parsed.source);
      }
      cursor = parsed.next;
      continue;
    }
    if (hasKeywordAt(source, cursor, "export")) {
      const exportStart = skipTrivia(source, cursor + "export".length);
      const typeOnly = hasKeywordAt(source, exportStart, "type");
      const declarationStart = typeOnly
        ? skipTrivia(source, exportStart + "type".length)
        : exportStart;
      if (
        source[declarationStart] !== "{" && source[declarationStart] !== "*"
      ) {
        cursor = declarationStart + 1;
        continue;
      }
      const parsed = parseFromModule(source, exportStart);
      if (
        parsed.source !== undefined && !typeOnly &&
        !isTypeOnlyModuleClause(parsed.clause)
      ) {
        runtimeSources.push(parsed.source);
      }
      cursor = parsed.next;
      continue;
    }
    cursor += 1;
  }

  return { dynamicImport, runtimeSources };
}

function runtimeModuleSources(source: string): readonly string[] {
  return scanModuleImports(source).runtimeSources;
}

function hasDynamicModuleImport(source: string): boolean {
  return scanModuleImports(source).dynamicImport ||
    DYNAMIC_IMPORT_TOKEN.test(source);
}

Deno.test("offline fixture accepts exactly two fictional data clients and only returns a withheld G5 result", () => {
  const executor = createOfflineFakeG5DesktopTwoClientExecutor(fixtureJson());

  const result = executor.consumeFixture();

  assertEquals(result.execution_authorization, "not-granted");
  assertEquals(result.g5_result, "not-assessed");
  assertEquals(
    assessG5DesktopTwoClientLifecycleEvidence(result.evidenceJson),
    {
      blockers: [],
      execution_authorization: "not-granted",
      g5_result: "not-assessed",
      lifecycle_validation: "accepted",
    },
  );
});

Deno.test("offline fixture accepts canonical client key order and distinct port/PID domains", () => {
  const crossDomainValue = fixture();
  crossDomainValue.clients[0].rootPid = crossDomainValue.clients[1].port;
  crossDomainValue.clients[0].rootProcessGeneration = `pid-${
    crossDomainValue.clients[0].rootPid
  }-generation-987654321`;

  const result = createOfflineFakeG5DesktopTwoClientExecutor(
    fixtureJson(crossDomainValue),
  ).consumeFixture();

  assertEquals(result.execution_authorization, "not-granted");
  assertEquals(result.g5_result, "not-assessed");
});

Deno.test("offline fixture rejects arbitrary functions without invoking them", () => {
  let invoked = false;
  const arbitraryFunction = () => {
    invoked = true;
    return fixtureJson();
  };
  const functionProperty = {
    value() {
      invoked = true;
      return fixtureJson();
    },
  };

  for (const input of [arbitraryFunction, functionProperty]) {
    assertThrows(
      () => createOfflineFakeG5DesktopTwoClientExecutor(input),
      Error,
      "G5 offline fixture input was rejected",
    );
  }
  assertEquals(invoked, false);
});

Deno.test("offline fixture rejects getters, Proxies, and foreign prototypes without observing them", () => {
  let getterTouched = false;
  const getterInput = Object.create(null) as Record<string, unknown>;
  Object.defineProperty(getterInput, "fixture", {
    enumerable: true,
    get() {
      getterTouched = true;
      return fixtureJson();
    },
  });

  let proxyTouched = false;
  const proxyInput = new Proxy({}, {
    get() {
      proxyTouched = true;
      return undefined;
    },
    getOwnPropertyDescriptor() {
      proxyTouched = true;
      return undefined;
    },
    getPrototypeOf() {
      proxyTouched = true;
      return null;
    },
    ownKeys() {
      proxyTouched = true;
      return [];
    },
  });

  let foreignTouched = false;
  const foreignInput = Object.create({
    toJSON() {
      foreignTouched = true;
      return fixture();
    },
  });

  for (const input of [getterInput, proxyInput, foreignInput]) {
    assertThrows(
      () => createOfflineFakeG5DesktopTwoClientExecutor(input),
      Error,
      "G5 offline fixture input was rejected",
    );
  }
  assertEquals(getterTouched, false);
  assertEquals(proxyTouched, false);
  assertEquals(foreignTouched, false);
});

Deno.test("offline fixture rejects duplicate identity, proof, PID, port, generation, and profile data before use", () => {
  const duplicateCases: MutableFixture[] = [];

  const duplicateClientId = fixture();
  duplicateClientId.clients[1].clientInstanceId =
    duplicateClientId.clients[0].clientInstanceId;
  duplicateCases.push(duplicateClientId);

  const duplicateCaptureProof = fixture();
  duplicateCaptureProof.clients[1].captureProofId =
    duplicateCaptureProof.clients[0].captureProofId;
  duplicateCases.push(duplicateCaptureProof);

  const duplicateTerminationProof = fixture();
  duplicateTerminationProof.clients[1].terminationProofId =
    duplicateTerminationProof.clients[0].terminationProofId;
  duplicateCases.push(duplicateTerminationProof);

  const duplicatePid = fixture();
  duplicatePid.clients[1].rootPid = duplicatePid.clients[0].rootPid;
  duplicatePid.clients[1].rootProcessGeneration =
    duplicatePid.clients[0].rootProcessGeneration;
  duplicateCases.push(duplicatePid);

  const duplicatePort = fixture();
  duplicatePort.clients[1].port = duplicatePort.clients[0].port;
  duplicateCases.push(duplicatePort);

  const duplicateGeneration = fixture();
  duplicateGeneration.clients[1].rootProcessGeneration =
    duplicateGeneration.clients[0].rootProcessGeneration;
  duplicateCases.push(duplicateGeneration);

  const duplicateProfileToken = fixture();
  duplicateProfileToken.clients[1].profileToken =
    duplicateProfileToken.clients[0].profileToken;
  duplicateCases.push(duplicateProfileToken);

  const overlappingPairIdentity = fixture();
  overlappingPairIdentity.pairId = RUN_ID;
  duplicateCases.push(overlappingPairIdentity);

  for (const value of duplicateCases) {
    assertThrows(
      () => createOfflineFakeG5DesktopTwoClientExecutor(fixtureJson(value)),
      Error,
      "G5 offline fixture input was rejected",
    );
  }
});

Deno.test("offline fixture rejects noncanonical or non-fictional input before construction", () => {
  const oneClient = fixture();
  const oneClientJson = JSON.stringify({
    ...oneClient,
    clients: [oneClient.clients[0]],
  });
  const threeClients = fixture();
  const threeClientJson = JSON.stringify({
    ...threeClients,
    clients: [
      ...threeClients.clients,
      client(
        "g5-client-c",
        4_203,
        28_293,
        "g5-profile-token-c",
        "g5-capture-proof-c",
        "g5-termination-proof-c",
      ),
    ],
  });
  const nonFictional = fixture();
  nonFictional.clients[1].fictional = false;
  const shortGeneration = fixture();
  shortGeneration.clients[0].rootProcessGeneration =
    "pid-4201-generation-12345678";
  const mismatchedGeneration = fixture();
  mismatchedGeneration.clients[0].rootProcessGeneration =
    "pid-4202-generation-987654321";
  const belowPortRange = fixture();
  belowPortRange.clients[0].port = 1_023;
  const abovePortRange = fixture();
  abovePortRange.clients[0].port = 65_536;
  const zeroRootPid = fixture();
  zeroRootPid.clients[0].rootPid = 0;
  const negativeRootPid = fixture();
  negativeRootPid.clients[0].rootPid = -1;
  const fractionalRootPid = fixture();
  fractionalRootPid.clients[0].rootPid = 4_201.5;
  const nonAscendingClientIds = fixture();
  nonAscendingClientIds.clients[0].clientInstanceId = "g5-client-z";
  const extraClientProperty = fixture();
  (extraClientProperty.clients[1] as unknown as Record<string, unknown>).extra =
    "unexpected";
  const missingClientProperty = fixture();
  delete (missingClientProperty.clients[1] as unknown as Record<
    string,
    unknown
  >).terminationProofId;
  const whitespaceVariant = ` ${fixtureJson()}`;

  for (
    const input of [
      oneClientJson,
      threeClientJson,
      legacyClientKeyOrderJson(),
      fixtureJson(nonFictional),
      fixtureJson(shortGeneration),
      fixtureJson(mismatchedGeneration),
      fixtureJson(belowPortRange),
      fixtureJson(abovePortRange),
      fixtureJson(zeroRootPid),
      fixtureJson(negativeRootPid),
      fixtureJson(fractionalRootPid),
      fixtureJson(nonAscendingClientIds),
      fixtureJson(extraClientProperty),
      fixtureJson(missingClientProperty),
      whitespaceVariant,
    ]
  ) {
    assertThrows(
      () => createOfflineFakeG5DesktopTwoClientExecutor(input),
      Error,
      "G5 offline fixture input was rejected",
    );
  }
});

Deno.test("offline fixture is single-use", () => {
  const executor = createOfflineFakeG5DesktopTwoClientExecutor(fixtureJson());

  executor.consumeFixture();
  assertThrows(
    () => executor.consumeFixture(),
    Error,
    "G5 offline fixture is single-use",
  );
});

Deno.test("offline fixture source scanner ignores type-only modules but retains runtime modules", () => {
  const moduleSource = [
    'import { RuntimeValue } from "./runtime.ts";',
    'import type { TypeOnly } from "./types.ts";',
    'import type DefaultType from "./default-type.ts";',
    'import { type InlineType } from "./inline-types.ts";',
    'export type { ExportedType } from "./export-types.ts";',
    'export { type InlineExportedType } from "./inline-export-types.ts";',
    'import { type InlineType, RuntimeValue } from "./mixed.ts";',
    'export { type InlineExportedType, RuntimeValue } from "./mixed-export.ts";',
    'export * from "./star.ts";',
  ].join("\n");

  assertEquals(
    runtimeModuleSources(moduleSource),
    [
      "./runtime.ts",
      "./mixed.ts",
      "./mixed-export.ts",
      "./star.ts",
    ],
  );
});

Deno.test("offline fixture source scanner rejects runtime imports despite trivia or semicolon evasions", () => {
  const moduleSource = [
    'import { RuntimeWithoutSemicolon } from "./named-without-semicolon.ts"',
    'import /* side-effect */ "./side-effect-without-semicolon.ts"',
    'import { RuntimeWithFromComment } from /* source */ "./from-comment.ts";',
    'import RuntimeWithAttributes from "./attributes.json" with { type: "json" };',
    'import /* dynamic */ ("./dynamic-comment.ts");',
    'import type * as TypeNamespace from "./type-namespace.ts";',
    'export type * as ExportedTypeNamespace from "./export-type-namespace.ts";',
    'import { /* comment */ type InlineType as InlineAlias } from "./commented-inline-type.ts";',
    'export { /* comment */ type InlineExportedType as InlineExportedAlias } from "./commented-inline-export-type.ts";',
    'import { type InlineType, RuntimeValue } from "./mixed.ts";',
    'export { type InlineExportedType, RuntimeValue } from "./mixed-export.ts";',
    'export * from "./star.ts";',
  ].join("\n");

  assertEquals(
    runtimeModuleSources(moduleSource),
    [
      "./named-without-semicolon.ts",
      "./side-effect-without-semicolon.ts",
      "./from-comment.ts",
      "./attributes.json",
      "./mixed.ts",
      "./mixed-export.ts",
      "./star.ts",
    ],
  );
  assertEquals(hasDynamicModuleImport(moduleSource), true);
  assertEquals(
    hasDynamicModuleImport(
      'const templateDynamic = `${import /* dynamic */("./template-dynamic.ts")}`;',
    ),
    true,
  );
});

Deno.test("offline fixture has no executable imports, capabilities, or live surface", async () => {
  const source = await Deno.readTextFile(
    new URL("./g5_desktop_two_client_executor.ts", import.meta.url),
  );

  assertEquals(
    runtimeModuleSources(source),
    ["./g5_desktop_two_client_lifecycle_contract.ts"],
  );
  assertEquals(/^\s*import\s+["'][^"']+["'];/mu.test(source), false);
  assertEquals(hasDynamicModuleImport(source), false);
  for (
    const forbidden of [
      "./browser_launcher.ts",
      "./g5_desktop_process_controller.ts",
      "createG5DesktopProcessController",
      "G5DesktopLaunchSupervisor",
      "Deno.",
      "eval(",
      "Function(",
      "fetch(",
      "WebSocket",
      "child_process",
      "require(",
      "startIsolatedBrowser",
      "createIsolatedBrowserLaunch",
      "new RegExp(",
      "credential",
      "password",
      "test-accounts",
      "process.env",
    ]
  ) {
    assertEquals(source.includes(forbidden), false, forbidden);
  }
});
