_("Make sure makeGUID makes guids of the right length/characters");
Cu.import("resource://weave/util.js");

function run_test() {
  // XXX this could cause random failures as guids arent always unique...
  _("Create a bunch of guids to make sure they don't conflict");
  let guids = [];
  for (let i = 0; i < 1000; i++) {
    let newGuid = Utils.makeGUID();
    _("Making sure guid has the right length without special characters:", newGuid);
    do_check_eq(encodeURIComponent(newGuid).length, 10);
    do_check_true(guids.every(function(g) g != newGuid));
    guids.push(newGuid);
  }
}
