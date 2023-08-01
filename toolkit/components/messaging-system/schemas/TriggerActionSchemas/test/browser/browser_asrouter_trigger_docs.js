const TEST_URL =
  "https://example.com/browser/toolkit/components/messaging-system/schemas/TriggerActionSchemas/test/browser/index.md";

const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
const { CFRMessageProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/CFRMessageProvider.sys.mjs"
);
const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "fetchTriggerActionSchema", async () => {
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
  const result = JsonSchema.validate(trigger, schema);
  if (result.errors.length) {
    throw new Error(
      `Trigger with id ${trigger.id} was not valid. Errors: ${JSON.stringify(
        result.errors,
        undefined,
        2
      )}`
    );
  }
  Assert.equal(
    result.errors.length,
    0,
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
  const messages = await CFRMessageProvider.getMessages();
  for (let message of messages) {
    await validateTrigger(message.trigger);
  }
});
