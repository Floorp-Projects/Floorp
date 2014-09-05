Components.utils.import("resource://gre/modules/Messaging.jsm");

let java = new JavaBridge(this);

do_register_cleanup(() => {
  java.disconnect();
});
do_test_pending();

function send_test_message(type) {
  let innerObject = {
    boolean: true,
    booleanArray: [false, true],
    int: 1,
    intArray: [2, 3],
    double: 0.5,
    doubleArray: [1.5, 2.5],
    null: null,
    emptyString: "",
    string: "foo",
    stringArray: ["bar", "baz"],
  }

  // Make a copy
  let outerObject = JSON.parse(JSON.stringify(innerObject));

  outerObject.type = type;
  outerObject.object = innerObject;
  outerObject.objectArray = [null, innerObject];

  Messaging.sendRequest(outerObject);
}

function send_message_for_response(type, response) {
  Messaging.sendRequestForResult({
    type: type,
    response: response,
  }).then(result => do_check_eq(result, response),
          error => do_check_eq(error, response));
}

function finish_test() {
  do_test_finished();
}
