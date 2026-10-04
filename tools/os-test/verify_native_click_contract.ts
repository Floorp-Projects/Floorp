#!/usr/bin/env -S deno run --allow-net --allow-env
// SPDX-License-Identifier: MPL-2.0

/**
 * Exercise the complete HTTP -> service -> actor -> native input path in a
 * running Floorp instance. The caller owns browser startup and its API port.
 *
 * deno run --allow-net --allow-env tools/os-test/verify_native_click_contract.ts \
 *   --base-url http://127.0.0.1:58261 --token-env NATIVE_CLICK_TEST_TOKEN
 *
 * A private ephemeral HTTP fixture and new automation instances are the only
 * resources created here. No existing tab, browser, server, or profile is used
 * as a fixture. Both visible tabs and hidden scraper instances run by default;
 * use --service tabs or --service scraper to select one.
 */

import { parseArgs } from "@std/cli/parse-args";

type Service = "tabs" | "scraper";
type JsonRecord = Record<string, unknown>;
type ResponseData = { status: number; body: JsonRecord };
type ObservedEvent = {
  type: string;
  button: number;
  detail: number;
  trusted: boolean;
  generation: string;
};

const FIXTURE = `<!doctype html>
<meta charset="utf-8">
<title>Native click contract fixture</title>
<style>
  html, body { margin: 0; width: 100%; min-height: 500px; }
  #mount { position: absolute; left: 80px; top: 90px; width: 240px; height: 140px; }
  button { width: 160px; height: 60px; }
  textarea { position: absolute; left: 10px; top: 350px; width: 350px; height: 100px; }
  @keyframes moving { from { transform: translateX(0); } to { transform: translateX(160px); } }
</style>
<div id="mount"></div><textarea id="events" aria-label="Recorded input"></textarea>
<input id="fixture-ready" type="hidden">
<script>
  const scenario = new URL(location.href).searchParams.get('scenario') || 'normal';
  const mount = document.getElementById('mount');
  const output = document.getElementById('events');
  const events = [];
  output.value = '[]';
  const makeButton = (generation = 'original') => {
    // GetText fingerprints block tags; a button's display:block CSS does not
    // make it a fingerprinted Markdown block.
    const button = document.createElement(scenario === 'fingerprint' ? 'div' : 'button');
    button.id = 'target';
    button.textContent = 'Native click target';
    button.dataset.generation = generation;
    button.style.cssText = 'display:block;width:160px;height:60px';
    return button;
  };
  let root = mount;
  if (scenario.startsWith('shadow') || scenario === 'nested-shadow' || scenario === 'slotted-label') {
    root = mount.attachShadow({ mode: 'open' });
    if (scenario === 'nested-shadow') {
      const innerHost = document.createElement('div');
      innerHost.style.cssText = 'display:block;width:200px;height:100px';
      root.appendChild(innerHost);
      root = innerHost.attachShadow({ mode: 'open' });
    }
  }
  const target = makeButton();
  root.appendChild(target);
  if (scenario === 'slotted-label') {
    target.textContent = '';
    target.style.padding = '0';
    target.appendChild(document.createElement('slot'));
    const label = document.createElement('span');
    label.id = 'slotted-label';
    label.textContent = 'Slotted native click target';
    label.style.cssText = 'display:block;width:100%;height:100%;line-height:56px';
    mount.appendChild(label);
  }
  if (scenario === 'page-pointer-events-none') document.body.style.pointerEvents = 'none';
  if (scenario === 'disabled') target.disabled = true;
  if (scenario === 'moving') target.style.animation = 'moving 0.5s linear infinite alternate';
  if (scenario === 'ambiguous') {
    target.className = 'ambiguous';
    const duplicate = makeButton();
    duplicate.id = 'duplicate';
    duplicate.className = 'ambiguous';
    root.appendChild(duplicate);
  }
  if (scenario === 'shadow-obscured') {
    const overlay = document.createElement('div');
    overlay.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,.1);z-index:99';
    document.body.appendChild(overlay);
  }
  if (scenario === 'shadow-inner-obscured') {
    const overlay = document.createElement('div');
    overlay.style.cssText = 'position:absolute;inset:0;background:rgba(0,0,0,.1);z-index:99';
    root.appendChild(overlay);
  }
  for (const type of ['mousemove', 'mousedown', 'mouseup', 'click', 'dblclick', 'auxclick', 'contextmenu']) {
    document.addEventListener(type, event => {
      const originalTarget = event.composedPath().find(node => node.id === 'target' || node.id === 'duplicate');
      if (!originalTarget) return;
      events.push({ type, button: event.button, detail: event.detail,
        trusted: event.isTrusted, generation: originalTarget.dataset.generation });
      output.value = JSON.stringify(events);
      if (type === 'contextmenu' || type === 'auxclick') event.preventDefault();
      if (scenario === 'replace-on-move' && type === 'mousemove' && originalTarget === target) {
        target.replaceWith(makeButton('replacement'));
      }
      if (scenario === 'replace-after-click' && type === 'click' && originalTarget === target) {
        target.replaceWith(makeButton('replacement'));
      }
    }, true);
  }
  document.documentElement.dataset.ready = 'true';
  document.getElementById('fixture-ready').value = location.href;
</script>`;

function record(value: unknown): JsonRecord {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error(`Expected JSON object, received ${JSON.stringify(value)}`);
  }
  return value as JsonRecord;
}

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function equal(actual: unknown, expected: unknown, message: string): void {
  assert(
    JSON.stringify(actual) === JSON.stringify(expected),
    `${message}: expected ${JSON.stringify(expected)}, got ${
      JSON.stringify(actual)
    }`,
  );
}

function diagnostic(response: ResponseData): JsonRecord {
  equal(response.status, 200, "Diagnostic HTTP status");
  const result = response.body;
  equal(result.backend, "window-synthesizeMouseEvent", "Native backend");
  assert(
    typeof result.reason === "string",
    "Diagnostic reason must be a string",
  );
  assert(typeof result.phase === "string", "Actor must return its phase");
  assert(
    typeof result.inputStarted === "boolean",
    "Actor must return input state",
  );
  assert(
    typeof result.activationStarted === "boolean",
    "Actor must return activation state",
  );
  return result;
}

function dispatched(response: ResponseData): void {
  const result = diagnostic(response);
  equal(result.ok, true, "Diagnostic success");
  equal(result.status, "dispatched", "Diagnostic status");
  equal(result.phase, "complete", "Completed sequence phase");
  equal(result.inputStarted, true, "Input began");
  equal(result.activationStarted, true, "Activation began");
}

export async function main(argv = Deno.args): Promise<number> {
  const args = parseArgs(argv, {
    string: ["base-url", "token-env", "service"],
    default: { "base-url": "http://127.0.0.1:58261", service: "all" },
  });
  const baseUrl = String(args["base-url"]).replace(/\/$/, "");
  const services: Service[] = args.service === "all"
    ? ["tabs", "scraper"]
    : args.service === "tabs" || args.service === "scraper"
    ? [args.service]
    : [];
  assert(services.length > 0, "--service must be all, tabs, or scraper");
  const tokenName = args["token-env"];
  const token = tokenName ? Deno.env.get(tokenName) : undefined;
  assert(
    !tokenName || token,
    "The requested token environment variable is empty",
  );

  const request = async (
    path: string,
    method: "GET" | "POST" | "DELETE" = "GET",
    data?: JsonRecord,
  ): Promise<ResponseData> => {
    const headers = new Headers({ "content-type": "application/json" });
    if (token) headers.set("authorization", `Bearer ${token}`);
    const response = await fetch(`${baseUrl}${path}`, {
      method,
      headers,
      body: data === undefined ? undefined : JSON.stringify(data),
      signal: AbortSignal.timeout(15_000),
    });
    return { status: response.status, body: record(await response.json()) };
  };

  const server = Deno.serve(
    { hostname: "127.0.0.1", port: 0, onListen() {} },
    () =>
      new Response(FIXTURE, {
        headers: {
          "content-type": "text/html; charset=utf-8",
          "cache-control": "no-store",
        },
      }),
  );
  const fixtureUrl = `http://127.0.0.1:${server.addr.port}/`;
  let passed = 0;
  let failed = 0;
  let sequence = 0;

  const run = async (name: string, fn: () => Promise<void>) => {
    try {
      await fn();
      passed++;
      console.log(`[PASS] ${name}`);
    } catch (error) {
      failed++;
      console.error(
        `[FAIL] ${name}: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    }
  };

  try {
    for (const service of services) {
      let instanceId: string | undefined;
      try {
        const created = await request(
          `/${service}/instances`,
          "POST",
          service === "tabs"
            ? { url: fixtureUrl, inBackground: false, waitForLoad: true }
            : {},
        );
        equal(created.status, 200, `${service}: create HTTP status`);
        assert(
          typeof created.body.instanceId === "string",
          `${service}: missing instance ID`,
        );
        instanceId = created.body.instanceId;
        const base = `/${service}/instances/${encodeURIComponent(instanceId)}`;
        const setup = async (scenario = "normal") => {
          const targetUrl = `${fixtureUrl}?scenario=${
            encodeURIComponent(scenario)
          }&run=${++sequence}`;
          const nav = await request(`${base}/navigate`, "POST", {
            url: targetUrl,
          });
          equal(nav.status, 200, "Fixture navigation");
          // Navigation may resolve on location change before the new actor is
          // ready. Poll only reads, requiring both the current browser URI and
          // a script-written marker for this exact document before any action.
          // A previous page's generic ready selector must not satisfy this.
          const deadline = performance.now() + 10_000;
          let lastUri: unknown;
          let lastReady: unknown;
          while (performance.now() < deadline) {
            const uri = await request(`${base}/uri`);
            equal(uri.status, 200, "Read current fixture URI");
            lastUri = uri.body.uri;
            if (lastUri === targetUrl) {
              const ready = await request(
                `${base}/value?selector=%23fixture-ready`,
              );
              equal(ready.status, 200, "Read fixture readiness marker");
              lastReady = ready.body.value;
              if (lastReady === targetUrl) return;
            }
            await new Promise((resolve) => setTimeout(resolve, 100));
          }
          throw new Error(
            `Fixture readiness deadline: expected ${targetUrl}; current URI=${
              String(lastUri)
            }, ready marker=${String(lastReady)}`,
          );
        };
        const click = (options: JsonRecord = {}) =>
          request(`${base}/click`, "POST", {
            selector: "#target",
            timeout: 1500,
            ...options,
          });
        const events = async (): Promise<ObservedEvent[]> => {
          const response = await request(`${base}/value?selector=%23events`);
          equal(response.status, 200, "Read fixture events");
          assert(
            typeof response.body.value === "string",
            "Fixture event value is a string",
          );
          const value: unknown = JSON.parse(response.body.value);
          assert(Array.isArray(value), "Fixture event value is an array");
          return value as ObservedEvent[];
        };
        const count = (observed: ObservedEvent[], type: string) =>
          observed.filter((event) => event.type === type).length;
        const noActivation = async () => {
          const observed = await events();
          equal(
            observed.filter((event) => event.type !== "mousemove"),
            [],
            "No mouse activation or fallback",
          );
        };

        await run(
          `${service}: legacy response and single trusted activation`,
          async () => {
            await setup();
            const response = await click();
            equal(response.status, 200, "Legacy HTTP status");
            equal(
              response.body,
              { ok: true },
              "Legacy response stays exactly {ok:boolean}",
            );
            const observed = await events();
            equal(count(observed, "mousedown"), 1, "Single mousedown");
            equal(count(observed, "mouseup"), 1, "Single mouseup");
            equal(
              count(observed, "click"),
              1,
              "Single click without DOM fallback",
            );
            assert(
              observed.every((event) => event.trusted),
              "All input must be trusted",
            );
          },
        );

        for (
          const [button, presses, clicks, doubleClicks, auxClicks] of [
            ["left", 2, 2, 1, 0],
            ["right", 1, 0, 0, 1],
            ["middle", 1, 0, 0, 1],
          ] as const
        ) {
          await run(
            `${service}: ${button} ${
              presses === 2 ? "double" : "single"
            } native sequence`,
            async () => {
              await setup();
              dispatched(
                await click({
                  button,
                  clickCount: presses,
                  includeResult: true,
                }),
              );
              const observed = await events();
              equal(count(observed, "mousedown"), presses, "Mousedown count");
              equal(count(observed, "mouseup"), presses, "Mouseup count");
              equal(count(observed, "click"), clicks, "Click count");
              equal(
                count(observed, "dblclick"),
                doubleClicks,
                "Double click count",
              );
              equal(
                count(observed, "auxclick"),
                auxClicks,
                "Auxiliary click count",
              );
              const expectedButton = { left: 0, middle: 1, right: 2 }[button];
              const activations = observed.filter((event) =>
                event.type !== "mousemove"
              );
              assert(
                activations.every((event) => event.button === expectedButton),
                "Native button identity",
              );
              assert(
                observed.every((event) => event.trusted),
                "All input must be trusted",
              );
              if (presses === 2) {
                equal(
                  activations.filter((event) => event.type === "click").map((
                    event,
                  ) => event.detail),
                  [1, 2],
                  "Native click detail progression",
                );
              }
            },
          );
        }

        await run(
          `${service}: invalid options rejected before input`,
          async () => {
            await setup();
            for (
              const invalid of [
                { includeResult: "yes" },
                { timeout: -1 },
                { timeout: "1" },
                { stabilityTimeout: -1 },
                { stabilityTimeout: null },
                { force: 1 },
                { button: "other" },
                { clickCount: 0 },
                { clickCount: 3 },
                { clickCount: 1.5 },
              ]
            ) {
              const response = await click(invalid);
              equal(
                response.status,
                400,
                `Invalid options ${JSON.stringify(invalid)}`,
              );
            }
            await noActivation();
          },
        );

        await run(
          `${service}: selector priority and fingerprint compatibility`,
          async () => {
            await setup("fingerprint");
            const text = await request(`${base}/text`);
            assert(
              typeof text.body.text === "string",
              "Text extraction must contain fingerprints",
            );
            let fingerprint: string | undefined;
            for (
              const match of text.body.text.matchAll(
                /<!--fp:([a-z0-9]{8}(?:[a-z0-9]{8})?)-->/g,
              )
            ) {
              const resolved = await request(
                `${base}/resolveFingerprint?fingerprint=${match[1]}`,
              );
              if (resolved.body.selector === "#target") {
                fingerprint = match[1];
                break;
              }
            }
            assert(
              fingerprint,
              `Target must have a resolvable fingerprint; extracted Markdown: ${text.body.text}`,
            );
            dispatched(
              await request(`${base}/click`, "POST", {
                fingerprint,
                includeResult: true,
              }),
            );
            dispatched(
              await click({ fingerprint: "invalid!", includeResult: true }),
            );
            equal(
              count(await events(), "click"),
              2,
              "Exactly one activation per resolved request",
            );
            equal(
              (await request(`${base}/click`, "POST", {
                fingerprint: "invalid!",
              })).status,
              400,
              "Invalid fingerprint rejected",
            );
          },
        );

        for (const scenario of ["shadow", "nested-shadow", "slotted-label"]) {
          await run(
            `${service}: ${scenario} target receives native input`,
            async () => {
              await setup(scenario);
              dispatched(await click({ includeResult: true }));
              const observed = await events();
              equal(count(observed, "click"), 1, "Shadow target clicked once");
              assert(
                observed.every((event) => event.trusted),
                "Shadow input must be trusted",
              );
            },
          );
        }

        for (
          const scenario of [
            "shadow-obscured",
            "shadow-inner-obscured",
            "page-pointer-events-none",
            "disabled",
            "moving",
          ]
        ) {
          await run(
            `${service}: ${scenario} expires without activation`,
            async () => {
              await setup(scenario);
              const started = performance.now();
              const result = diagnostic(
                await click({
                  includeResult: true,
                  timeout: 250,
                  stabilityTimeout: 200,
                }),
              );
              equal(result.ok, false, "Expired action fails");
              equal(result.status, "refused", "Expired action refused");
              equal(
                result.reason,
                "deadline-exceeded",
                "Single action deadline",
              );
              equal(result.inputStarted, false, "No input before eligibility");
              equal(
                result.activationStarted,
                false,
                "No activation before eligibility",
              );
              assert(
                performance.now() - started < 2500,
                "Short action deadline stays bounded including HTTP overhead",
              );
              await noActivation();
            },
          );
        }

        await run(`${service}: ambiguity refused before input`, async () => {
          await setup("ambiguous");
          const result = diagnostic(
            await click({ selector: ".ambiguous", includeResult: true }),
          );
          equal(result.status, "refused", "Ambiguous target refused");
          equal(result.reason, "ambiguous-selector", "Ambiguity diagnostic");
          equal(result.inputStarted, false, "No input for ambiguous target");
          await noActivation();
        });

        await run(
          `${service}: zero deadline preserves legacy false response`,
          async () => {
            await setup();
            equal(
              (await click({ timeout: 0 })).body,
              { ok: false },
              "Legacy refusal response",
            );
            await noActivation();
          },
        );

        await run(
          `${service}: mousemove replacement refuses before mousedown`,
          async () => {
            await setup("replace-on-move");
            const result = diagnostic(await click({ includeResult: true }));
            equal(result.status, "refused", "Changed target refused");
            equal(
              result.reason,
              "eligibility-changed-before-mousedown",
              "Changed target diagnostic",
            );
            equal(result.inputStarted, true, "Movement started");
            equal(result.activationStarted, false, "Activation did not start");
            await noActivation();
          },
        );

        await run(
          `${service}: replacement after first click prevents second activation`,
          async () => {
            await setup("replace-after-click");
            const result = diagnostic(
              await click({ includeResult: true, clickCount: 2 }),
            );
            equal(result.ok, false, "Partial double click is not success");
            equal(
              result.status,
              "unknown",
              "Partial activation cannot be reported as safe refusal",
            );
            equal(
              result.reason,
              "eligibility-changed-after-activation",
              "Partial activation diagnostic",
            );
            equal(
              result.activationStarted,
              true,
              "Partial activation recorded",
            );
            const observed = await events();
            equal(count(observed, "mousedown"), 1, "Only first mousedown");
            equal(count(observed, "mouseup"), 1, "Only first mouseup");
            equal(count(observed, "click"), 1, "No second click or fallback");
            equal(count(observed, "dblclick"), 0, "No completed double click");
            assert(
              observed.every((event) => event.generation === "original"),
              "Replacement never receives input",
            );
          },
        );
      } catch (error) {
        failed++;
        console.error(
          `[FAIL] ${service}: setup: ${
            error instanceof Error ? error.message : String(error)
          }`,
        );
      } finally {
        if (instanceId) {
          const ownedInstanceId = instanceId;
          await run(
            `${service}: remove owned automation instance`,
            async () => {
              const base = `/${service}/instances/${
                encodeURIComponent(ownedInstanceId)
              }`;
              const response = service === "tabs"
                ? await request(`${base}/close`, "POST")
                : await request(base, "DELETE");
              equal(response.status, 200, "Owned instance cleanup");
              equal(response.body.ok, true, "Owned instance cleanup result");
            },
          );
        }
      }
    }
  } finally {
    await server.shutdown();
  }
  console.log(`Native click integration: ${passed} passed, ${failed} failed`);
  return failed > 0 ? 1 : 0;
}

if (import.meta.main) Deno.exit(await main());
