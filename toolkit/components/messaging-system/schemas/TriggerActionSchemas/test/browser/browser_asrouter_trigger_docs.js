const TEST_URL =
  "https://example.com/browser/toolkit/components/messaging-system/schemas/TriggerActionSchemas/test/browser/TriggerActionSchemas.md";

const { TriggerActionSchemas } = ChromeUtils.import(
  "resource://testing-common/TriggerActionSchemas.js"
);
const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "JsonSchemaValidator",
  "resource://gre/modules/components-utils/JsonSchemaValidator.jsm"
);

// TODO: docs
async function validateTrigger(trigger) {
  const schema = TriggerActionSchemas[trigger.id];
  ok(schema, `should have a schema for ${trigger.id}`);
  const { valid, error } = JsonSchemaValidator.validate(trigger, schema);
  if (!valid) {
    throw new Error(
      `Trigger with id ${trigger.id} was not valid: ${error.message}`
    );
  }
  ok(valid, `should be a valid action of type ${trigger.id}`);
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
      `${triggerName} not found in trigger-listeners.md`
    );
  }
});

add_task(async function test_message_triggers() {
  const messages = CFRMessageProvider.getMessages();
  for (let message of messages) {
    if (message.id === "MILESTONE_MESSAGE") {
      // JsonSchemaValidator.jsm doesn't support mixed schema definitions.
      // `contentBlocking` CFRs all have integer params as arguments except
      // this one which we can't correctly validate with the schema.
      continue;
    }
    validateTrigger(message.trigger);
  }
});
