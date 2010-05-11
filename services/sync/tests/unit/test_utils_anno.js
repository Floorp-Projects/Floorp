_("Make sure various combinations of anno arguments do the right get/set for pages/items");
Cu.import("resource://weave/util.js");

function run_test() {
  _("set an anno on an item 1");
  Utils.anno(1, "anno", "hi");
  do_check_eq(Utils.anno(1, "anno"), "hi");

  _("set an anno on a url");
  Utils.anno("about:", "tation", "hello");
  do_check_eq(Utils.anno("about:", "tation"), "hello");

  _("make sure getting it also works with a nsIURI");
  let uri = Utils.makeURI("about:");
  do_check_eq(Utils.anno(uri, "tation"), "hello");

  _("make sure annotations get updated");
  Utils.anno(uri, "tation", "bye!");
  do_check_eq(Utils.anno("about:", "tation"), "bye!");

  _("sanity check that the item anno is still there");
  do_check_eq(Utils.anno(1, "anno"), "hi");
}
