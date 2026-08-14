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

Deno.test("offline fixture has no executable imports, capabilities, or live surface", async () => {
  const source = await Deno.readTextFile(
    new URL("./g5_desktop_two_client_executor.ts", import.meta.url),
  );

  const runtimeModuleSources = [
    ...source.matchAll(
      /^\s*(?:import|export)\s+(?!type\b)[\s\S]*?\sfrom\s+["']([^"']+)["'];/gmu,
    ),
  ].map((match) => match[1]);
  assertEquals(
    runtimeModuleSources,
    ["./g5_desktop_two_client_lifecycle_contract.ts"],
  );
  assertEquals(/^\s*import\s+["'][^"']+["'];/mu.test(source), false);
  assertEquals(/\bimport\s*\(/u.test(source), false);
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
