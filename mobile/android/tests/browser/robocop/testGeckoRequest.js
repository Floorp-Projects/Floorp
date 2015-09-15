Components.utils.import("resource://gre/modules/Messaging.jsm");

var java = new JavaBridge(this);

do_register_cleanup(() => {
  java.disconnect();
});
do_test_pending();

function add_request_listener(message) {
  Messaging.addListener(function (data) {
    return { result: data + "bar" };
  }, message);
}

function add_exception_listener(message) {
  Messaging.addListener(function (data) {
    throw "error!";
  }, message);
}

function add_second_request_listener(message) {
  let exceptionCaught = false;

  try {
    Messaging.addListener(() => {}, message);
  } catch (e) {
    exceptionCaught = true;
  }

  do_check_true(exceptionCaught);
}

function remove_request_listener(message) {
  Messaging.removeListener(message);
}

function finish_test() {
  do_test_finished();
}
