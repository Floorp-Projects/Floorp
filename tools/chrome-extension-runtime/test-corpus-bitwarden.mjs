#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { snapshotUI } from "./corpus-sidepanel-ui.sys.mjs";
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { createServer } from "node:https";
import { createInterface } from "node:readline/promises";
import {
  chmodSync,
  existsSync,
  mkdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
/** @param {string} name @param {string} [fallback] */
const option = (name, fallback = "") => {
  const index = process.argv.indexOf(name);
  return index < 0 ? fallback : process.argv[index + 1];
};
const serverFile = option("--server");
if (!serverFile) {
  throw Error(
    "--server /path/to/server.json is required; run prepare-bitwarden-vault.py first",
  );
}
/** @type {import("./sidepanel-test-types.ts").VaultInfo} */
const serverInfo = JSON.parse(readFileSync(serverFile).toString("utf8"));
const output = dirname(resolve(serverFile));
/** @type {import("./sidepanel-test-types.ts").VaultCredentials} */
const credentials = JSON.parse(
  readFileSync(join(output, "private-credentials.json")).toString("utf8"),
);
const originalId = "nngceckbapebfimnlniiiahkandclblb";
/** @type {import("./corpus-types.ts").FrozenCorpus} */
const corpus = JSON.parse(
  readFileSync(join(root, "tools/chrome-extension-runtime/corpus-lock.json"))
    .toString("utf8"),
);
const frozen = corpus.extensions.find((extension) =>
  extension.id === originalId
);
/** @type {import("./sidepanel-test-types.ts").ReferenceMetadata} */
if (!frozen) throw Error("Frozen Bitwarden selection missing");
const baseline = JSON.parse(
  readFileSync(
    join(root, "tools/chrome-extension-runtime/chrome-reference.json"),
  ).toString("utf8"),
);
const chrome = join(
  root,
  "_dist/chrome-reference/extracted/opt/google/chrome/chrome",
);
/** @param {import("node:crypto").BinaryLike} value */
const digest = (value) => createHash("sha256").update(value).digest("hex");
if (digest(readFileSync(chrome)) !== baseline.binary.sha256) {
  throw Error("Frozen Chrome binary mismatch");
}
if (
  digest(
    readFileSync(
      join(root, `_dist/chrome-extension-corpus/packages/${originalId}.crx`),
    ),
  ) !== frozen.packageSha256
) throw Error("Frozen Bitwarden package mismatch");
if (
  new URL(serverInfo.url).hostname !== "localhost" ||
  !serverInfo.url.startsWith("https://")
) throw Error("The ephemeral test vault must use HTTPS localhost");
/** @type {import("./sidepanel-test-types.ts").SourceIntegrity} */
const sourceIntegrity = JSON.parse(
  readFileSync(join(output, "source-integrity.json")).toString("utf8"),
);
if (
  !Array.isArray(sourceIntegrity.scripts) || !sourceIntegrity.scripts.length
) throw Error("Canonical signed-script integrity ledger required");
for (const entry of sourceIntegrity.scripts) {
  const separator = entry.lastIndexOf(":");
  const name = entry.slice(0, separator), expected = entry.slice(separator + 1);
  if (
    digest(readFileSync(join(output, "chrome-bitwarden-original", name))) !==
      expected
  ) throw Error("Original signed script changed: " + name);
}
if (!frozen) throw Error("Frozen Bitwarden selection missing");
const tls = join(output, "data/tls");
if (!existsSync(join(tls, "authored.pem"))) {
  /** @param {string[]} args */
  const quiet = (args) => execFileSync("openssl", args, { stdio: "ignore" });
  quiet([
    "req",
    "-newkey",
    "rsa:2048",
    "-nodes",
    "-keyout",
    join(tls, "authored-key.pem"),
    "-out",
    join(tls, "authored.csr"),
    "-subj",
    "/CN=corpus.floorp.test",
  ]);
  writeFileSync(
    join(tls, "authored.ext"),
    "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:corpus.floorp.test\n",
  );
  quiet([
    "x509",
    "-req",
    "-in",
    join(tls, "authored.csr"),
    "-CA",
    join(tls, "ca.pem"),
    "-CAkey",
    join(tls, "ca-key.pem"),
    "-CAcreateserial",
    "-out",
    join(tls, "authored.pem"),
    "-days",
    "3",
    "-extfile",
    join(tls, "authored.ext"),
  ]);
}
chmodSync(join(tls, "authored-key.pem"), 0o600);
const fixtureBytes = readFileSync(
  join(
    root,
    "tools/chrome-extension-runtime/fixtures/corpus-bitwarden-login.html",
  ),
);
const fixture = createServer({
  key: readFileSync(join(tls, "authored-key.pem")),
  cert: readFileSync(join(tls, "authored.pem")),
}, (_request, response) => {
  response.writeHead(200, {
    "Content-Type": "text/html; charset=utf-8",
    "Cache-Control": "no-store",
  });
  response.end(fixtureBytes);
});
await new Promise((resolve) =>
  fixture.listen(0, "127.0.0.1", () => resolve(undefined))
);
const address = fixture.address();
if (!address || typeof address === "string") {
  throw Error("Authored TLS server has no TCP address");
}
const authoredURL = `https://corpus.floorp.test:${address.port}/login`;
credentials.authoredURL = authoredURL;
credentials.vaultURL = serverInfo.url;
const run = join(
  output,
  option("--run-name", "chrome-primary-reference") ||
    "chrome-primary-reference",
);
mkdirSync(run, { recursive: true });
const proxy = process.env.HTTPS_PROXY || process.env.HTTP_PROXY;
const browser = new ChromePipe(join(output, "chrome-local-trust"), run, [
  "--host-resolver-rules=MAP corpus.floorp.test 127.0.0.1",
  "--proxy-bypass-list=localhost;127.0.0.1;corpus.floorp.test",
  ...(proxy ? [`--proxy-server=${proxy}`] : ["--no-proxy-server"]),
], { headed: true });
/** @type {import("./sidepanel-test-types.ts").VaultReport} */
const report = {
  stage: "actual-Bitwarden-primary-operation",
  passed: false,
  reference: baseline,
  extension: {
    id: originalId,
    version: frozen.version,
    packageSha256: frozen.packageSha256,
  },
  server: {
    image: serverInfo.image,
    version: serverInfo.version,
    url: serverInfo.url,
  },
  authoredURL,
  authoredSourceSha256: digest(fixtureBytes),
  tlsValidation:
    "normal X.509 validation; private trust store; no certificate-ignore flags",
  steps: [],
  sourceIntegrity,
};
const save = () =>
  writeFileSync(join(run, "report.json"), JSON.stringify(report, null, 2));
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @type {string|undefined} */
let popupSession;
/** @type {string|undefined} */
let authorSession;
/** @type {string|undefined} */
let authorTab;
/** @template T @param {string|undefined} session @param {string} expression @returns {Promise<T>} */
const evaluate = async (session, expression) => {
  const result = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
    awaitPromise: true,
  }, session);
  if (result.exceptionDetails) {
    throw Error(
      result.exceptionDetails.exception?.description ||
        result.exceptionDetails.text,
    );
  }
  return /** @type {T} */ (result.result.value);
};
/** @template T @param {()=>Promise<T>} predicate @param {string} label @returns {Promise<NonNullable<T>>} */
const wait = async (predicate, label) => {
  for (let count = 0; count < 100; count++) {
    const value = await predicate();
    if (value) return value;
    await pause(100);
  }
  throw Error("Timeout: " + label);
};
const ui = async () => {
  /** @type {import("./sidepanel-test-types.ts").SidePanelSnapshot} */
  const value = await evaluate(
    popupSession,
    `({...(${snapshotUI.toString()})(),viewport:{width:innerWidth,height:innerHeight}})`,
  );
  value.url = value.url.split("?")[0];
  for (const control of value.controls) {
    if (control.type === "password") control.value = "[redacted]";
  }
  return value;
};
/** @param {import("./sidepanel-test-types.ts").ProbeCommand} command */
const click = async (command) => {
  const selector = JSON.stringify(command.selector || "button, a");
  const label = command.text === undefined
    ? "true"
    : `element.textContent.trim()===${
      JSON.stringify(command.text)
    } || element.getAttribute('aria-label')===${JSON.stringify(command.text)}`;
  /** @type {import("./sidepanel-test-types.ts").PopupPoint|null} */
  const position = await evaluate(
    popupSession,
    `(()=>{const element=[...document.querySelectorAll(${selector})].find(element=>{const rect=element.getBoundingClientRect();return rect.width&&rect.height&&(${label});});if(!element)return null;element.scrollIntoView({block:'center',behavior:'instant'});const rect=element.getBoundingClientRect();return {x:rect.x+rect.width/2,y:rect.y+rect.height/2,viewportWidth:innerWidth,viewportHeight:innerHeight};})()`,
  );
  if (!position) {
    if (command.optional) return { skipped: true };
    throw Error(
      "Visible control unavailable: " + (command.selector || command.text),
    );
  }
  report.lastClick = {
    selector: command.selector,
    text: command.text,
    ...position,
  };
  if (
    position.x < 0 || position.y < 0 || position.x >= position.viewportWidth ||
    position.y >= position.viewportHeight
  ) {
    throw Error(
      "Visible control outside the actual native popup viewport: " +
        JSON.stringify(position),
    );
  }
  for (const type of ["mousePressed", "mouseReleased"]) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      x: position.x,
      y: position.y,
      button: "left",
      clickCount: 1,
    }, popupSession);
  }
};
const action = async () => {
  await browser.call("Target.activateTarget", { targetId: authorTab });
  const native =
    (await browser.call("Target.getTargets", { filter: [{ type: "tab" }] }))
      .targetInfos.find((target) => target.url === authoredURL);
  if (!native) throw Error("Actual authored tab unavailable");
  await browser.call("Extensions.triggerAction", {
    id: originalId,
    targetId: native.targetId,
  });
  const popup = await wait(
    async () =>
      (await browser.call("Target.getTargets")).targetInfos.find((target) =>
        target.type === "page" &&
        target.url.startsWith(`chrome-extension://${originalId}/popup/`)
      ),
    "native action popup",
  );
  popupSession = (await browser.call("Target.attachToTarget", {
    targetId: popup.targetId,
    flatten: true,
  })).sessionId;
  await wait(
    () => evaluate(popupSession, "document.body?.innerText?.trim().length>3"),
    "authored Bitwarden UI",
  );
};
try {
  const version = await browser.call("Browser.getVersion");
  if (version.product !== `Chrome/${baseline.browserVersion}`) {
    throw Error("Reference version mismatch");
  }
  report.runtime = version;
  const loaded = await browser.call("Extensions.loadUnpacked", {
    path: join(output, "chrome-bitwarden-original"),
  });
  if (loaded.id !== originalId) throw Error("Original Chrome identity changed");
  await pause(6500);
  for (const target of (await browser.call("Target.getTargets")).targetInfos) {
    if (
      target.type === "page" && !target.url.startsWith("chrome-extension://")
    ) await browser.call("Target.closeTarget", { targetId: target.targetId });
  }
  authorTab =
    (await browser.call("Target.createTarget", { url: authoredURL })).targetId;
  authorSession = (await browser.call("Target.attachToTarget", {
    targetId: authorTab,
    flatten: true,
  })).sessionId;
  await wait(
    () =>
      evaluate(
        authorSession,
        "document.querySelector('#authored-password')!==null",
      ),
    "controlled HTTPS login document",
  );
  const empty = await evaluate(
    authorSession,
    "document.querySelector('#authored-username').value===''&&document.querySelector('#authored-password').value===''&&isSecureContext",
  );
  if (!empty) {
    throw Error("Authored fixture did not start empty under normal TLS");
  }
  report.initialFieldsEmpty = true;
  await action();
  save();
  await pause(1000);
  const initialUI = await ui();
  writeFileSync(
    join(run, "initial-ui.json"),
    JSON.stringify(initialUI, null, 2),
  );
  console.log(JSON.stringify({ ready: true, authoredURL, ui: initialUI }));
  const recipe = option("--recipe");
  const input = recipe ? null : createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false,
  });
  /** @type {AsyncIterable<string>|string[]|null} */
  const lines = recipe
    ? /** @type {import("./sidepanel-test-types.ts").ProbeCommand[]} */ (JSON
      .parse(readFileSync(resolve(recipe)).toString("utf8"))).map((command) =>
        JSON.stringify(command)
      )
    : input;
  if (!lines) throw Error("Native command input missing");
  for await (const line of lines) {
    if (!line.trim()) continue;
    try {
      /** @type {import("./sidepanel-test-types.ts").ProbeCommand} */
      const command = JSON.parse(line);
      if (command.op === "close") break;
      if (command.op === "click") await click(command);
      else if (command.op === "fill") {
        const value = command.valueFrom
          ? credentials[command.valueFrom]
          : command.text;
        if (typeof value !== "string") throw Error("Unknown authored input");
        await evaluate(
          popupSession,
          `document.querySelector(${JSON.stringify(command.selector)}).focus()`,
        );
        await browser.call("Input.insertText", { text: value }, popupSession);
      } else if (command.op === "wait") {
        await wait(
          () =>
            evaluate(
              popupSession,
              `Array.from(document.querySelectorAll(${
                JSON.stringify(command.selector || "button,a")
              })).some(element => { const rect = element.getBoundingClientRect(); return rect.width && rect.height && (${
                command.text
                  ? "element.textContent.trim() === " +
                    JSON.stringify(command.text) +
                    " || element.getAttribute('aria-label') === " +
                    JSON.stringify(command.text)
                  : "true"
              }); })`,
            ),
          "visible extension UI",
        );
      } else if (command.op === "action") await action();
      else if (command.op === "verify-fill") {
        /** @type {import("./sidepanel-test-types.ts").FilledValues} */
        const values = await evaluate(
          authorSession,
          "({username:document.querySelector('#authored-username').value,password:document.querySelector('#authored-password').value})",
        );
        report.passed = values.username === credentials.username &&
          values.password === credentials.password;
        report.fillMatches = {
          username: values.username === credentials.username,
          password: values.password === credentials.password,
        };
      } else if (command.op !== "snapshot") throw Error("Unknown operation");
      report.steps.push(command);
      await pause(command.waitMs || 250);
      save();
      const state = await ui().catch(() => ({ closed: true }));
      writeFileSync(
        join(run, `ui-${report.steps.length}.json`),
        JSON.stringify(state, null, 2),
      );
      console.log(
        JSON.stringify({ ok: true, passed: report.passed, ui: state }),
      );
    } catch (error) {
      console.log(JSON.stringify({ ok: false, error: String(error) }));
      if (recipe) throw error;
    }
  }
  input?.close();
  if (recipe && report.passed !== true) {
    throw Error("Actual Bitwarden primary operation was not completed");
  }
} catch (error) {
  report.error = String(error);
  throw error;
} finally {
  save();
  await browser.close();
  fixture.close();
}
