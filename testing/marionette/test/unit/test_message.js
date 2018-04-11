/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  InvalidArgumentError,
  WebDriverError,
} = ChromeUtils.import("chrome://marionette/content/error.js", {});
ChromeUtils.import("chrome://marionette/content/message.js");

add_test(function test_Message_Origin() {
  equal(0, Message.Origin.Client);
  equal(1, Message.Origin.Server);

  run_next_test();
});

add_test(function test_Message_fromPacket() {
  let cmd = new Command(4, "foo");
  let resp = new Response(5, () => {});
  resp.error = "foo";

  ok(Message.fromPacket(cmd.toPacket()) instanceof Command);
  ok(Message.fromPacket(resp.toPacket()) instanceof Response);
  Assert.throws(() => Message.fromPacket([3, 4, 5, 6]),
      /Unrecognised message type in packet/);

  run_next_test();
});

add_test(function test_Command() {
  let cmd = new Command(42, "foo", {bar: "baz"});
  equal(42, cmd.id);
  equal("foo", cmd.name);
  deepEqual({bar: "baz"}, cmd.parameters);
  equal(null, cmd.onerror);
  equal(null, cmd.onresult);
  equal(Message.Origin.Client, cmd.origin);
  equal(false, cmd.sent);

  run_next_test();
});

add_test(function test_Command_onresponse() {
  let onerrorOk = false;
  let onresultOk = false;

  let cmd = new Command(7, "foo");
  cmd.onerror = () => onerrorOk = true;
  cmd.onresult = () => onresultOk = true;

  let errorResp = new Response(8, () => {});
  errorResp.error = new WebDriverError("foo");

  let bodyResp = new Response(9, () => {});
  bodyResp.body = "bar";

  cmd.onresponse(errorResp);
  equal(true, onerrorOk);
  equal(false, onresultOk);

  cmd.onresponse(bodyResp);
  equal(true, onresultOk);

  run_next_test();
});

add_test(function test_Command_ctor() {
  let cmd = new Command(42, "bar", {bar: "baz"});
  let msg = cmd.toPacket();

  equal(Command.Type, msg[0]);
  equal(cmd.id, msg[1]);
  equal(cmd.name, msg[2]);
  equal(cmd.parameters, msg[3]);

  run_next_test();
});

add_test(function test_Command_toString() {
  let cmd = new Command(42, "foo", {bar: "baz"});
  equal(JSON.stringify(cmd.toPacket()), cmd.toString());

  run_next_test();
});

add_test(function test_Command_fromPacket() {
  let c1 = new Command(42, "foo", {bar: "baz"});

  let msg = c1.toPacket();
  let c2 = Command.fromPacket(msg);

  equal(c1.id, c2.id);
  equal(c1.name, c2.name);
  equal(c1.parameters, c2.parameters);

  Assert.throws(() => Command.fromPacket([null, 2, "foo", {}]));
  Assert.throws(() => Command.fromPacket([1, 2, "foo", {}]));
  Assert.throws(() => Command.fromPacket([0, null, "foo", {}]));
  Assert.throws(() => Command.fromPacket([0, 2, null, {}]));
  Assert.throws(() => Command.fromPacket([0, 2, "foo", false]));

  let nullParams = Command.fromPacket([0, 2, "foo", null]);
  equal("[object Object]", Object.prototype.toString.call(nullParams.parameters));

  run_next_test();
});

add_test(function test_Command_Type() {
  equal(0, Command.Type);
  run_next_test();
});

add_test(function test_Response_ctor() {
  let handler = () => run_next_test();

  let resp = new Response(42, handler);
  equal(42, resp.id);
  equal(null, resp.error);
  ok("origin" in resp);
  equal(Message.Origin.Server, resp.origin);
  equal(false, resp.sent);
  equal(handler, resp.respHandler_);

  run_next_test();
});

add_test(function test_Response_sendConditionally() {
  let fired = false;
  let resp = new Response(42, () => fired = true);
  resp.sendConditionally(() => false);
  equal(false, resp.sent);
  equal(false, fired);
  resp.sendConditionally(() => true);
  equal(true, resp.sent);
  equal(true, fired);

  run_next_test();
});

add_test(function test_Response_send() {
  let fired = false;
  let resp = new Response(42, () => fired = true);
  resp.send();
  equal(true, resp.sent);
  equal(true, fired);

  run_next_test();
});

add_test(function test_Response_sendError_sent() {
  let resp = new Response(42, r => equal(false, r.sent));
  resp.sendError(new WebDriverError());
  ok(resp.sent);
  Assert.throws(() => resp.send(), /already been sent/);

  run_next_test();
});

add_test(function test_Response_sendError_body() {
  let resp = new Response(42, r => equal(null, r.body));
  resp.sendError(new WebDriverError());

  run_next_test();
});

add_test(function test_Response_sendError_errorSerialisation() {
  let err1 = new WebDriverError();
  let resp1 = new Response(42);
  resp1.sendError(err1);
  equal(err1.status, resp1.error.error);
  deepEqual(err1.toJSON(), resp1.error);

  let err2 = new InvalidArgumentError();
  let resp2 = new Response(43);
  resp2.sendError(err2);
  equal(err2.status, resp2.error.error);
  deepEqual(err2.toJSON(), resp2.error);

  run_next_test();
});

add_test(function test_Response_sendError_wrapInternalError() {
  let err = new ReferenceError("foo");

  // errors that originate from JavaScript (i.e. Marionette implementation
  // issues) should be converted to UnknownError for transport
  let resp = new Response(42, r => {
    equal("unknown error", r.error.error);
    equal(false, resp.sent);
  });

  // they should also throw after being sent
  Assert.throws(() => resp.sendError(err), /foo/);
  equal(true, resp.sent);

  run_next_test();
});

add_test(function test_Response_toPacket() {
  let resp = new Response(42, () => {});
  let msg = resp.toPacket();

  equal(Response.Type, msg[0]);
  equal(resp.id, msg[1]);
  equal(resp.error, msg[2]);
  equal(resp.body, msg[3]);

  run_next_test();
});

add_test(function test_Response_toString() {
  let resp = new Response(42, () => {});
  resp.error = "foo";
  resp.body = "bar";

  equal(JSON.stringify(resp.toPacket()), resp.toString());

  run_next_test();
});

add_test(function test_Response_fromPacket() {
  let r1 = new Response(42, () => {});
  r1.error = "foo";
  r1.body = "bar";

  let msg = r1.toPacket();
  let r2 = Response.fromPacket(msg);

  equal(r1.id, r2.id);
  equal(r1.error, r2.error);
  equal(r1.body, r2.body);

  Assert.throws(() => Response.fromPacket([null, 2, "foo", {}]));
  Assert.throws(() => Response.fromPacket([0, 2, "foo", {}]));
  Assert.throws(() => Response.fromPacket([1, null, "foo", {}]));
  Assert.throws(() => Response.fromPacket([1, 2, null, {}]));
  Response.fromPacket([1, 2, "foo", null]);

  run_next_test();
});

add_test(function test_Response_Type() {
  equal(1, Response.Type);
  run_next_test();
});
