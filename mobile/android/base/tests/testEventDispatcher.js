Components.utils.import("resource://gre/modules/Messaging.jsm");

let java = new JavaBridge(this);

do_register_cleanup(() => {
  java.disconnect();
});
do_test_pending();

function send_test_message(type) {
  sendMessageToJava({
    type: type,
    boolean: true,
    int: 1,
    double: 0.5,
    string: "foo",
    object: {
      boolean: true,
      int: 1,
      double: 0.5,
      string: "foo",
    },
  });
}

function send_message_for_response(type, response) {
  sendMessageToJava({
    type: type,
    response: response,
  }, (success, error) => {
    if (response === "success") {
      do_check_eq(success, response);
      do_check_eq(error, null);
    } else if (response === "error") {
      do_check_eq(success, null);
      do_check_eq(error, response);
    } else {
      do_throw("Unexpected response: " + response);
    }
  });
}

function finish_test() {
  do_test_finished();
}
