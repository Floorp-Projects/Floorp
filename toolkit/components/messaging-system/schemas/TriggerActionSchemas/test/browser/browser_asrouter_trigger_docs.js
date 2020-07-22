const TEST_URL =
  "https://example.com/browser/toolkit/components/messaging-system/schemas/TriggerActionSchemas/test/browser/index.md";

const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);
const { Ajv } = ChromeUtils.import("resource://testing-common/ajv-4.1.1.js");

XPCOMUtils.defineLazyGetter(this, "fetchTriggerActionSchema", async () => {
  const response = await fetch(
    "resource://testing-common/TriggerActionSchemas.json"
  );
  const schema = await response.json();
  if (!schema) {
    throw new Error("Failed to load TriggerActionSchemas");
  }
  return schema.definitions.TriggerActionSchemas;
});

async function validateTrigger(trigger) {
  const schema = await fetchTriggerActionSchema;
  const ajv = new Ajv({ async: "co*" });
  const validator = ajv.compile(schema);
  if (!validator(trigger)) {
    throw new Error(`Trigger with id ${trigger.id} was not valid.`);
  }
  Assert.ok(
    !validator.errors,
    `should be a valid trigger of type ${trigger.id}`
  );
}

function getHeadingsFromDocs(docs) {
  const re = /### `(\w+)`/g;
  const found = [];
  let match = 1;
  while (match) {
    match = re.exec(docs);
    if (match) {
      found.push(match[1]);
    }
  }
  return found;
}

add_task(async function test_trigger_docs() {
  let request = await fetch(TEST_URL, { credentials: "omit" });
  let docs = await request.text();
  let headings = getHeadingsFromDocs(docs);
  for (let triggerName of ASRouterTriggerListeners.keys()) {
    Assert.ok(
      headings.includes(triggerName),
      `${triggerName} not found in TriggerActionSchemas/index.md`
    );
  }
});

add_task(async function test_message_triggers() {
  const messages = CFRMessageProvider.getMessages();
  for (let message of messages) {
    await validateTrigger(message.trigger);
  }
});
