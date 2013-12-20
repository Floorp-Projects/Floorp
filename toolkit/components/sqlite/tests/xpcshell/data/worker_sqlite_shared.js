/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function send(message) {
  self.postMessage(message);
}

function do_test_complete() {
  send({kind: "do_test_complete", args: []});
}

function do_check_true(x) {
  send({kind: "do_check_true", args: [!!x]});
  if (x) {
    dump("TEST-PASS: " + x + "\n");
  } else {
    throw new Error("do_check_true failed");
  }
}

function do_check_eq(a, b) {
  let result = a == b;
  send({kind: "do_check_true", args: [result]});
  if (!result) {
    throw new Error("do_check_eq failed " + a + " != " + b);
  }
}

function do_check_neq(a, b) {
 let result = a != b;
 send({kind: "do_check_true", args: [result]});
 if (!result) {
   throw new Error("do_check_neq failed " + a + " == " + b);
 }
}

function do_print(x) {
  dump("TEST-INFO: " + x + "\n");
}
