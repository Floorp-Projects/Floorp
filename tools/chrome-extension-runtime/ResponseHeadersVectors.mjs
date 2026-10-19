// SPDX-License-Identifier: MPL-2.0
// The same authored vectors run against real Chrome155 and Gecko APIs/HTTP.
export async function runResponseHeaderVectors(api, origin) {
  const dnr = api.declarativeNetRequest;
  const checks = [];
  const observations = [];
  let sequence = 0;
  const assert = (condition, label) => {
    if (!condition) {
      const error = Error(label);
      error.checks = checks;
      error.observations = observations;
      throw error;
    }
    checks.push(label);
  };
  const set = async rules => {
    const old = await dnr.getDynamicRules();
    await dnr.updateDynamicRules({
      removeRuleIds: old.map(rule => rule.id),
      addRules: rules,
    });
  };
  const url = headers =>
    `${origin}headers?sequence=${++sequence}&h=${encodeURIComponent(JSON.stringify(headers))}`;
  const rule = (
    condition,
    action = {
      type: "modifyHeaders",
      responseHeaders: [
        { header: "x-matched", operation: "set", value: "yes" },
      ],
    },
    id = 1,
    priority = 1
  ) => ({
    id,
    priority,
    condition: {
      urlFilter: "|" + origin + "headers?",
      resourceTypes: ["xmlhttprequest"],
      ...condition,
    },
    action,
  });
  async function match(label, condition, headers, expected) {
    await set([rule(condition)]);
    const response = await fetch(url(headers));
    const matched = response.headers.get("x-matched") === "yes";
    observations.push({ label, condition, headers, matched });
    assert(matched === expected, label);
  }
  await match(
    "Header names are insensitive to ASCII case",
    { responseHeaders: [{ header: "X-PrObE" }] },
    { "x-probe": ["alpha"] },
    true
  );
  await match(
    "Absent included header does not match",
    { responseHeaders: [{ header: "x-absent" }] },
    {},
    false
  );
  await match(
    "HeaderInfo alternatives are OR conditions",
    { responseHeaders: [{ header: "x-absent" }, { header: "x-probe" }] },
    { "x-probe": ["alpha"] },
    true
  );
  await match(
    "Included header values use ASCII-insensitive glob",
    { responseHeaders: [{ header: "x-probe", values: ["aL*HA"] }] },
    { "x-probe": ["Alpha"] },
    true
  );
  await match(
    "Question wildcard matches zero characters",
    { responseHeaders: [{ header: "x-probe", values: ["a?b"] }] },
    { "x-probe": ["ab"] },
    true
  );
  await match(
    "Question wildcard matches one character",
    { responseHeaders: [{ header: "x-probe", values: ["a?b"] }] },
    { "x-probe": ["acb"] },
    true
  );
  await match(
    "Question wildcard excludes two characters",
    { responseHeaders: [{ header: "x-probe", values: ["a?b"] }] },
    { "x-probe": ["accb"] },
    false
  );
  await match(
    "Escaped wildcard is literal",
    { responseHeaders: [{ header: "x-probe", values: ["a\\*b\\?"] }] },
    { "x-probe": ["a*b?"] },
    true
  );
  await match(
    "Mixed wildcard follows Chrome's left-to-right segment matching",
    { responseHeaders: [{ header: "x-probe", values: ["*a?b"] }] },
    { "x-probe": ["aacb"] },
    false
  );
  await match(
    "Excluded values take precedence over included values",
    {
      responseHeaders: [
        { header: "x-probe", values: ["a*"], excludedValues: ["alpha"] },
      ],
    },
    { "x-probe": ["alpha"] },
    false
  );
  await match(
    "Excluded values alone require the header to exist",
    { responseHeaders: [{ header: "x-probe", excludedValues: ["beta"] }] },
    {},
    false
  );
  await match(
    "Excluded values alone match another header value",
    { responseHeaders: [{ header: "x-probe", excludedValues: ["beta"] }] },
    { "x-probe": ["alpha"] },
    true
  );
  await match(
    "Empty values list matches header existence",
    { responseHeaders: [{ header: "x-probe", values: [] }] },
    { "x-probe": ["alpha"] },
    true
  );
  await match(
    "Repeated response header values are individually matched",
    { responseHeaders: [{ header: "x-probe", values: ["beta"] }] },
    { "x-probe": ["alpha", "beta"] },
    true
  );
  await match(
    "Comma-separated response header values are individually matched",
    { responseHeaders: [{ header: "x-probe", values: ["beta"] }] },
    { "x-probe": ["alpha, beta"] },
    true
  );
  await match(
    "Quoted commas remain within one header value",
    { responseHeaders: [{ header: "x-probe", values: ['"alpha,beta"'] }] },
    { "x-probe": ['"alpha,beta", gamma'] },
    true
  );
  await match(
    "Non-coalescing header keeps its comma",
    {
      responseHeaders: [{ header: "set-cookie", values: ["test=alpha, beta"] }],
    },
    { "set-cookie": ["test=alpha, beta"] },
    true
  );
  await match(
    "Excluded response header blocks an included alternative",
    {
      responseHeaders: [{ header: "x-probe" }],
      excludedResponseHeaders: [{ header: "x-probe", values: ["alpha"] }],
    },
    { "x-probe": ["alpha"] },
    false
  );
  await match(
    "Excluded header absence matches at response stage",
    { excludedResponseHeaders: [{ header: "x-absent" }] },
    { "x-probe": ["alpha"] },
    true
  );
  await match(
    "Actual Stylus content-type XML exclusion",
    {
      excludedResponseHeaders: [{ header: "content-type", values: ["*/xml*"] }],
    },
    { "content-type": ["application/xml"] },
    false
  );
  await match(
    "Actual Stylus content-type text exclusion",
    {
      responseHeaders: [
        {
          header: "content-type",
          values: ["text/*"],
          excludedValues: ["text/html*"],
        },
      ],
    },
    { "content-type": ["text/css"] },
    true
  );
  await set([
    rule({ responseHeaders: [{ header: "x-probe" }] }, { type: "block" }),
  ]);
  let blocked = false;
  const blockedUrl = url({ "x-probe": ["alpha"] });
  observations.push({
    serverMustSee: new URL(blockedUrl).pathname + new URL(blockedUrl).search,
  });
  try {
    await fetch(blockedUrl);
  } catch {
    blocked = true;
  }
  assert(
    blocked,
    "Response-stage block rejects actual HTTP response after it reached server"
  );
  await set([
    rule(
      { responseHeaders: [{ header: "x-probe" }] },
      { type: "redirect", redirect: { url: origin + "redirected" } }
    ),
  ]);
  const redirectSource = url({ "x-probe": ["alpha"] });
  observations.push({
    serverMustSee:
      new URL(redirectSource).pathname + new URL(redirectSource).search,
  });
  const redirected = await fetch(redirectSource);
  assert(
    redirected.url === origin + "redirected",
    "Response-stage redirect reaches actual destination"
  );
  await set([
    rule({}, { type: "allow" }, 1, 10),
    rule({ responseHeaders: [{ header: "x-probe" }] }, { type: "block" }, 2, 1),
  ]);
  assert(
    (await fetch(url({ "x-probe": ["alpha"] }))).ok,
    "Earlier higher-priority allow suppresses response-stage block"
  );
  await set([
    rule({}, undefined, 1, 1),
    rule(
      { responseHeaders: [{ header: "x-probe" }] },
      { type: "allow" },
      2,
      10
    ),
  ]);
  assert(
    !(await fetch(url({ "x-probe": ["alpha"] }))).headers.has("x-matched"),
    "Response-stage allow suppresses lower-priority earlier response modifications"
  );
  await set([
    rule(
      {},
      {
        type: "modifyHeaders",
        responseHeaders: [
          { header: "x-probe", operation: "set", value: "changed" },
        ],
      },
      1,
      10
    ),
    rule(
      { responseHeaders: [{ header: "x-probe", values: ["original"] }] },
      undefined,
      2,
      20
    ),
  ]);
  assert(
    (await fetch(url({ "x-probe": ["original"] }))).headers.get("x-matched") ===
      "yes",
    "Matching observes original response headers before any rule modifications"
  );
  await set([
    {
      id: 1,
      priority: 20,
      condition: {
        urlFilter: "|" + origin + "frame",
        resourceTypes: ["sub_frame"],
        responseHeaders: [{ header: "x-allow" }],
      },
      action: { type: "allowAllRequests" },
    },
    {
      id: 2,
      priority: 1,
      condition: {
        urlFilter: "|" + origin + "child",
        resourceTypes: ["xmlhttprequest"],
      },
      action: { type: "block" },
    },
  ]);
  async function frameAllowed(allow) {
    const key = Math.random().toString(16).slice(2);
    const frame = document.createElement("iframe");
    const promise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        window.removeEventListener("message", listener);
        reject(Error("Frame header condition timed out"));
      }, 8000);
      function listener(event) {
        if (event.source !== frame.contentWindow || event.data?.key !== key)
          return;
        window.removeEventListener("message", listener);
        clearTimeout(timeout);
        resolve(event.data.allowed);
      }
      window.addEventListener("message", listener);
    });
    frame.src = origin + "frame?allow=" + Number(allow) + "&key=" + key;
    document.body.append(frame);
    try {
      return await promise;
    } finally {
      frame.remove();
    }
  }
  assert(
    await frameAllowed(true),
    "Response-conditioned ancestor allowAllRequests allows real child HTTP request"
  );
  assert(
    !(await frameAllowed(false)),
    "Ancestor with absent header cannot allow a later child request"
  );
  const invalid = [
    rule({ responseHeaders: [] }),
    rule({ excludedResponseHeaders: [] }),
    rule({ responseHeaders: [{ header: "bad header" }] }),
    rule({
      responseHeaders: [{ header: "x-probe", values: ["bad\r\nvalue"] }],
    }),
    rule({
      responseHeaders: [{ header: "x-probe" }],
      excludedResponseHeaders: [{ header: "x-probe" }],
    }),
    rule(
      { responseHeaders: [{ header: "x-probe" }] },
      {
        type: "modifyHeaders",
        requestHeaders: [
          { header: "x-illegal", operation: "set", value: "late" },
        ],
      }
    ),
  ];
  await set([rule({})]);
  for (const [index, malformed] of invalid.entries()) {
    let rejected = false;
    try {
      await dnr.updateDynamicRules({
        removeRuleIds: [1],
        addRules: [malformed],
      });
    } catch {
      rejected = true;
    }
    const stored = await dnr.getDynamicRules();
    observations.push({ label: "invalid atomic " + index, rejected, stored });
    assert(
      rejected && stored[0]?.condition.responseHeaders == null,
      `Invalid header rule ${index + 1} rejects atomically`
    );
  }
  await set([rule({ excludedResponseHeaders: [{ header: "x-absent" }] })]);
  const outcome = await dnr.testMatchOutcome({
    url: origin + "headers?test",
    type: "xmlhttprequest",
  });
  assert(
    outcome.matchedRules.some(match => match.ruleId === 1),
    "testMatchOutcome evaluates absent response headers at the response stage"
  );
  await set([
    rule({ responseHeaders: [{ header: "x-probe", values: ["alpha"] }] }),
  ]);
  const supplied = await dnr.testMatchOutcome({
    url: origin + "headers?test",
    type: "xmlhttprequest",
    responseHeaders: { "x-probe": ["alpha"] },
  });
  assert(
    supplied.matchedRules.some(match => match.ruleId === 1),
    "testMatchOutcome consumes genuine HeaderInfo responseHeaders dictionary"
  );
  const exported = await dnr.getDynamicRules();
  assert(
    !("regexFilter" in exported[0].condition) &&
      !("requestHeaders" in exported[0].action),
    "Chrome rule getters omit absent optional fields rather than inserting null"
  );
  await set([]);
  return { checks, observations };
}
