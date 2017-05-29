Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Normalize passphrase recognizes hyphens.");
  const pp = "26ect2thczm599m2ffqarbicjq";
  const hyphenated = "2-6ect2-thczm-599m2-ffqar-bicjq";
  do_check_eq(Utils.normalizePassphrase(hyphenated), pp);

  _("Skip whitespace.");
  do_check_eq("aaaaaaaaaaaaaaaaaaaaaaaaaa", Utils.normalizePassphrase("aaaaaaaaaaaaaaaaaaaaaaaaaa  "));
  do_check_eq("aaaaaaaaaaaaaaaaaaaaaaaaaa", Utils.normalizePassphrase("	 aaaaaaaaaaaaaaaaaaaaaaaaaa"));
  do_check_eq("aaaaaaaaaaaaaaaaaaaaaaaaaa", Utils.normalizePassphrase("    aaaaaaaaaaaaaaaaaaaaaaaaaa  "));
  do_check_eq("aaaaaaaaaaaaaaaaaaaaaaaaaa", Utils.normalizePassphrase("    a-aaaaa-aaaaa-aaaaa-aaaaa-aaaaa  "));
  do_check_true(Utils.isPassphrase("aaaaaaaaaaaaaaaaaaaaaaaaaa  "));
  do_check_true(Utils.isPassphrase("	 aaaaaaaaaaaaaaaaaaaaaaaaaa"));
  do_check_true(Utils.isPassphrase("    aaaaaaaaaaaaaaaaaaaaaaaaaa  "));
  do_check_true(Utils.isPassphrase("    a-aaaaa-aaaaa-aaaaa-aaaaa-aaaaa  "));
  do_check_false(Utils.isPassphrase("    -aaaaa-aaaaa-aaaaa-aaaaa-aaaaa  "));

  _("Normalizing 20-char passphrases.");
  do_check_eq(Utils.normalizePassphrase("abcde-abcde-abcde-abcde"),
              "abcdeabcdeabcdeabcde");
  do_check_eq(Utils.normalizePassphrase("a-bcde-abcde-abcde-abcde"),
              "a-bcde-abcde-abcde-abcde");
  do_check_eq(Utils.normalizePassphrase(" abcde-abcde-abcde-abcde "),
              "abcdeabcdeabcdeabcde");
}
