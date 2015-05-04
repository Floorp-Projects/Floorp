let java = new JavaBridge(this);
let javaResponded = false;

do_register_cleanup(() => {
  java.disconnect();
});
do_test_pending();

function check_js_int_arg(int1) {
  // Sync call from Java
  do_check_eq(int1, 1);
  java.asyncCall("checkJavaIntArg", 2);
}

function check_js_double_arg(double3) {
  // Sync call from Java
  do_check_eq(double3, 3.0);
  java.asyncCall("checkJavaDoubleArg", 4.0);
}

function check_js_boolean_arg(boolfalse) {
  // Sync call from Java
  do_check_eq(boolfalse, false);
  java.asyncCall("checkJavaBooleanArg", true);
}

function check_js_string_arg(stringfoo) {
  do_check_eq(stringfoo, "foo");
  java.asyncCall("checkJavaStringArg", "bar");
}

function check_js_object_arg(obj) {
  // Sync call from Java
  do_check_eq(obj.caller, "java");
  java.asyncCall("checkJavaObjectArg", {caller: "js"});
}

function check_js_sync_call() {
  // Sync call from Java
  java.syncCall("doJSSyncCall");
  // respond_to_js_sync_call should have run by now because
  // do_js_sync_call calls it from Java code
  do_check_true(javaResponded);

  java.asyncCall("checkJSSyncCallReceived");
  // End of test
  do_test_finished();
}

function respond_to_js_sync_call() {
  javaResponded = true;
}
