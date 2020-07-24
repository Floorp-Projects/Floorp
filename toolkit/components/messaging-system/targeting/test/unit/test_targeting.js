const { TargetingContext } = ChromeUtils.import(
  "resource://messaging-system/targeting/Targeting.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

add_task(async function instance_with_default() {
  let targeting = new TargetingContext();

  let res = await targeting.eval(
    `ctx.locale == '${Services.locale.appLocaleAsBCP47}'`
  );

  Assert.ok(res, "Has local context");
});

add_task(async function instance_with_context() {
  let targeting = new TargetingContext({ bar: 42 });

  let res = await targeting.eval("ctx.bar == 42");

  Assert.ok(res, "Merge provided context with default");
});

add_task(async function eval_1_context() {
  let targeting = new TargetingContext();

  let res = await targeting.eval("custom1.bar == 42", { custom1: { bar: 42 } });

  Assert.ok(res, "Eval uses provided context");
});

add_task(async function eval_2_context() {
  let targeting = new TargetingContext();

  let res = await targeting.eval("custom1.bar == 42 && custom2.foo == 42", {
    custom1: { bar: 42 },
    custom2: { foo: 42 },
  });

  Assert.ok(res, "Eval uses provided context");
});

add_task(async function eval_multiple_context() {
  let targeting = new TargetingContext();

  let res = await targeting.eval(
    "custom1.bar == 42 && custom2.foo == 42 && custom3.baz == 42",
    { custom1: { bar: 42 }, custom2: { foo: 42 } },
    { custom3: { baz: 42 } }
  );

  Assert.ok(res, "Eval uses provided context");
});

add_task(async function eval_multiple_context_precedence() {
  let targeting = new TargetingContext();

  let res = await targeting.eval(
    "custom1.bar == 42 && custom2.foo == 42",
    { custom1: { bar: 24 }, custom2: { foo: 24 } },
    { custom1: { bar: 42 }, custom2: { foo: 42 } }
  );

  Assert.ok(res, "Last provided context overrides previously defined ones.");
});

add_task(async function eval_evalWithDefault() {
  let targeting = new TargetingContext({ foo: 42 });

  let res = await targeting.evalWithDefault("foo == 42");

  Assert.ok(res, "Eval uses provided context");
});

add_task(async function log_targeting_error_events() {
  let ctx = {
    get foo() {
      throw new Error("unit test");
    },
  };
  let targeting = new TargetingContext(ctx);
  let stub = sinon.stub(targeting, "_sendUndesiredEvent");

  await Assert.rejects(
    targeting.evalWithDefault("foo == 42", ctx),
    /unit test/,
    "Getter should throw"
  );

  Assert.equal(stub.callCount, 1, "Error event was logged");
  let {
    args: [{ event, value }],
  } = stub.firstCall;
  Assert.equal(event, "attribute_error", "Correct error message");
  Assert.equal(value, "foo", "Correct attribute name");
});

add_task(async function eval_evalWithDefault_precedence() {
  let targeting = new TargetingContext({ region: "space" });
  let res = await targeting.evalWithDefault("region != 'space'");

  Assert.ok(res, "Custom context does not override TargetingEnvironment");
});

add_task(async function eval_evalWithDefault_combineContexts() {
  let combinedCtxs = TargetingContext.combineContexts({ foo: 1 }, { foo: 2 });
  let targeting = new TargetingContext(combinedCtxs);
  let res = await targeting.evalWithDefault("foo == 1");

  Assert.ok(res, "First match is returned for combineContexts");
});

add_task(async function log_targeting_error_events_in_namespace() {
  let ctx = {
    get foo() {
      throw new Error("unit test");
    },
  };
  let targeting = new TargetingContext(ctx);
  let stub = sinon.stub(targeting, "_sendUndesiredEvent");
  let catchStub = sinon.stub();

  try {
    await targeting.eval("ctx.foo == 42");
  } catch (e) {
    catchStub();
  }

  Assert.equal(stub.callCount, 1, "Error event was logged");
  let {
    args: [{ event, value }],
  } = stub.firstCall;
  Assert.equal(event, "attribute_error", "Correct error message");
  Assert.equal(value, "ctx.foo", "Correct attribute name");
  Assert.ok(catchStub.calledOnce, "eval throws errors");
});

add_task(async function log_timeout_errors() {
  let ctx = {
    timeout: 1,
    get foo() {
      return new Promise(() => {});
    },
  };

  let targeting = new TargetingContext(ctx);
  let stub = sinon.stub(targeting, "_sendUndesiredEvent");
  let catchStub = sinon.stub();

  try {
    await targeting.eval("ctx.foo");
  } catch (e) {
    catchStub();
  }

  Assert.equal(catchStub.callCount, 1, "Timeout error throws");
  Assert.equal(stub.callCount, 1, "Timeout event was logged");
  let {
    args: [{ event, value }],
  } = stub.firstCall;
  Assert.equal(event, "attribute_timeout", "Correct error message");
  Assert.equal(value, "ctx.foo", "Correct attribute name");
});

add_task(async function test_telemetry_event_timeout() {
  Services.telemetry.clearEvents();
  let ctx = {
    timeout: 1,
    get foo() {
      return new Promise(() => {});
    },
  };
  let expectedEvents = [
    ["messaging_experiments", "targeting", "attribute_timeout", "ctx.foo"],
  ];
  let targeting = new TargetingContext(ctx);

  try {
    await targeting.eval("ctx.foo");
  } catch (e) {}

  TelemetryTestUtils.assertEvents(expectedEvents);
  Services.telemetry.clearEvents();
});

add_task(async function test_telemetry_event_error() {
  Services.telemetry.clearEvents();
  let ctx = {
    get bar() {
      throw new Error("unit test");
    },
  };
  let expectedEvents = [
    ["messaging_experiments", "targeting", "attribute_error", "ctx.bar"],
  ];
  let targeting = new TargetingContext(ctx);

  try {
    await targeting.eval("ctx.bar");
  } catch (e) {}

  TelemetryTestUtils.assertEvents(expectedEvents);
  Services.telemetry.clearEvents();
});
