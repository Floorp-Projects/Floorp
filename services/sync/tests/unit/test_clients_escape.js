/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
let _ = function(some, debug, text, to) print(Array.join(arguments, " "));

Components.utils.import("resource://weave/engines/clientData.js");

function run_test() {
  _("Test that serializing client records results in uploadable ascii");
  Clients.setInfo("ascii", {
    name: "wéävê"
  });

  _("Make sure we have the expected record");
  let record = Clients._store.createRecord("ascii");
  do_check_eq(record.id, "ascii");
  do_check_eq(record.payload.name, "wéävê");

  let serialized = record.serialize();
  let checkCount = 0;
  _("Checking for all ASCII:", serialized);
  Array.forEach(serialized, function(ch) {
    let code = ch.charCodeAt(0);
    _("Checking asciiness of '", ch, "'=", code);
    do_check_true(code < 128);
    checkCount++;
  });

  _("Processed", checkCount, "characters out of", serialized.length);
  do_check_eq(checkCount, serialized.length);

  _("Making sure the record still looks like it did before");
  do_check_eq(record.id, "ascii");
  do_check_eq(record.payload.name, "wéävê");

  _("Sanity check that creating the record also gives the same");
  record = Clients._store.createRecord("ascii");
  do_check_eq(record.id, "ascii");
  do_check_eq(record.payload.name, "wéävê");
}
