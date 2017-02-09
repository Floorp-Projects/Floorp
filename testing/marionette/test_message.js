/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/message.js");

add_test(function test_MessageOrigin() {
  equal(0, MessageOrigin.Client);
  equal(1, MessageOrigin.Server);

  run_next_test();
});

add_test(function test_Message_fromMsg() {
  let cmd = new Command(4, "foo");
  let resp = new Response(5, () => {});
  resp.error = "foo";

  ok(Message.fromMsg(cmd.toMsg()) instanceof Command);
  ok(Message.fromMsg(resp.toMsg()) instanceof Response);
  Assert.throws(() => Message.fromMsg([3, 4, 5, 6]),
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
  equal(MessageOrigin.Client, cmd.origin);
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
  let msg = cmd.toMsg();

  equal(Command.TYPE, msg[0]);
  equal(cmd.id, msg[1]);
  equal(cmd.name, msg[2]);
  equal(cmd.parameters, msg[3]);

  run_next_test();
});

add_test(function test_Command_toString() {
  let cmd = new Command(42, "foo", {bar: "baz"});
  equal(`Command {id: ${cmd.id}, ` +
      `name: ${JSON.stringify(cmd.name)}, ` +
      `parameters: ${JSON.stringify(cmd.parameters)}}`,
      cmd.toString());

  run_next_test();
});

add_test(function test_Command_fromMsg() {
  let c1 = new Command(42, "foo", {bar: "baz"});

  let msg = c1.toMsg();
  let c2 = Command.fromMsg(msg);

  equal(c1.id, c2.id);
  equal(c1.name, c2.name);
  equal(c1.parameters, c2.parameters);

  Assert.throws(() => Command.fromMsg([null, 2, "foo", {}]));
  Assert.throws(() => Command.fromMsg([1, 2, "foo", {}]));
  Assert.throws(() => Command.fromMsg([0, null, "foo", {}]));
  Assert.throws(() => Command.fromMsg([0, 2, null, {}]));
  Assert.throws(() => Command.fromMsg([0, 2, "foo", false]));

  let nullParams = Command.fromMsg([0, 2, "foo", null]);
  equal("[object Object]", Object.prototype.toString.call(nullParams.parameters));

  run_next_test();
});

add_test(function test_Command_TYPE() {
  equal(0, Command.TYPE);
  run_next_test();
});

add_test(function test_Response_ctor() {
  let handler = () => run_next_test();

  let resp = new Response(42, handler);
  equal(42, resp.id);
  equal(null, resp.error);
  ok("origin" in resp);
  equal(MessageOrigin.Server, resp.origin);
  equal(false, resp.sent);
  equal(handler, resp.respHandler_);

  run_next_test();
});

add_test(function test_Response_sendConditionally() {
  let fired = false;
  let resp = new Response(42, () => fired = true);
  resp.sendConditionally(r => false);
  equal(false, resp.sent);
  equal(false, fired);
  resp.sendConditionally(r => true);
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

add_test(function test_Response_sendError() {
  let err = new WebDriverError();
  let resp = new Response(42, r => {
    equal(err.toJSON().error, r.error.error);
    equal(null, r.body);
    equal(false, r.sent);
  });

  resp.sendError(err);
  equal(true, resp.sent);
  Assert.throws(() => resp.send(), /already been sent/);

  resp.sent = false;
  Assert.throws(() => resp.sendError(new Error()));

  run_next_test();
});

add_test(function test_Response_toMsg() {
  let resp = new Response(42, () => {});
  let msg = resp.toMsg();

  equal(Response.TYPE, msg[0]);
  equal(resp.id, msg[1]);
  equal(resp.error, msg[2]);
  equal(resp.body, msg[3]);

  run_next_test();
});

add_test(function test_Response_toString() {
  let resp = new Response(42, () => {});
  resp.error = "foo";
  resp.body = "bar";

  equal(`Response {id: ${resp.id}, ` +
      `error: ${JSON.stringify(resp.error)}, ` +
      `body: ${JSON.stringify(resp.body)}}`,
      resp.toString());

  run_next_test();
});

add_test(function test_Response_fromMsg() {
  let r1 = new Response(42, () => {});
  r1.error = "foo";
  r1.body = "bar";

  let msg = r1.toMsg();
  let r2 = Response.fromMsg(msg);

  equal(r1.id, r2.id);
  equal(r1.error, r2.error);
  equal(r1.body, r2.body);

  Assert.throws(() => Response.fromMsg([null, 2, "foo", {}]));
  Assert.throws(() => Response.fromMsg([0, 2, "foo", {}]));
  Assert.throws(() => Response.fromMsg([1, null, "foo", {}]));
  Assert.throws(() => Response.fromMsg([1, 2, null, {}]));
  Response.fromMsg([1, 2, "foo", null]);

  run_next_test();
});

add_test(function test_Response_TYPE() {
  equal(1, Response.TYPE);
  run_next_test();
});
