/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function log(text) {
  dump("WORKER " + text + "\n");
}

function send(message) {
  self.postMessage(message);
}

function finish() {
  send({kind: "finish"});
}

function ok(condition, description) {
  send({kind: "ok", condition: !!condition, description: "" + description});
}

function is(a, b, description) {
  let outcome = a == b; // Need to decide outcome here, as not everything can be serialized
  send({kind: "is", outcome, description: "" + description, a: "" + a, b: "" + b});
}

function isnot(a, b, description) {
  let outcome = a != b; // Need to decide outcome here, as not everything can be serialized
  send({kind: "isnot", outcome, description: "" + description, a: "" + a, b: "" + b});
}

function info(description) {
  send({kind: "info", description: "" + description});
}
