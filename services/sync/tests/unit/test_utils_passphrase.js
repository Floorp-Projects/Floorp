Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Generated passphrase has length 26.");
  let pp = Utils.generatePassphrase();
  do_check_eq(pp.length, 26);

  const key = "abcdefghijkmnpqrstuvwxyz23456789";
  _("Passphrase only contains [" + key + "].");
  do_check_true(pp.split('').every(function(chr) key.indexOf(chr) != -1));

  _("Hyphenated passphrase has 5 hyphens.");
  let hyphenated = Utils.hyphenatePassphrase(pp);
  _("H: " + hyphenated);
  do_check_eq(hyphenated.length, 31);
  do_check_eq(hyphenated[1], '-');
  do_check_eq(hyphenated[7], '-');
  do_check_eq(hyphenated[13], '-');
  do_check_eq(hyphenated[19], '-');
  do_check_eq(hyphenated[25], '-');
  do_check_eq(pp,
      hyphenated.slice(0, 1) + hyphenated.slice(2, 7)
      + hyphenated.slice(8, 13) + hyphenated.slice(14, 19)
      + hyphenated.slice(20, 25) + hyphenated.slice(26, 31));

  _("Arbitrary hyphenation.");
  // We don't allow invalid characters for our base32 character set.
  do_check_eq(Utils.hyphenatePassphrase("1234567"), "2-34567");  // Not partial, so no trailing dash.
  do_check_eq(Utils.hyphenatePassphrase("1234567890"), "2-34567-89");
  do_check_eq(Utils.hyphenatePassphrase("abcdeabcdeabcdeabcdeabcde"), "a-bcdea-bcdea-bcdea-bcdea-bcde");
  do_check_eq(Utils.hyphenatePartialPassphrase("1234567"), "2-34567-");
  do_check_eq(Utils.hyphenatePartialPassphrase("1234567890"), "2-34567-89");
  do_check_eq(Utils.hyphenatePartialPassphrase("abcdeabcdeabcdeabcdeabcde"), "a-bcdea-bcdea-bcdea-bcdea-bcde");

  do_check_eq(Utils.hyphenatePartialPassphrase("a"), "a-");
  do_check_eq(Utils.hyphenatePartialPassphrase("1234567"), "2-34567-");
  do_check_eq(Utils.hyphenatePartialPassphrase("a-bcdef-g"),
              "a-bcdef-g");
  do_check_eq(Utils.hyphenatePartialPassphrase("abcdefghijklmnop"),
              "a-bcdef-ghijk-mnp");
  do_check_eq(Utils.hyphenatePartialPassphrase("abcdefghijklmnopabcde"),
              "a-bcdef-ghijk-mnpab-cde");
  do_check_eq(Utils.hyphenatePartialPassphrase("a-bcdef-ghijk-LMNOP-ABCDE-Fg"),
              "a-bcdef-ghijk-mnpab-cdefg-");
  // Cuts off.
  do_check_eq(Utils.hyphenatePartialPassphrase("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").length, 31);

  _("Normalize passphrase recognizes hyphens.");
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

  _("Normalizing username.");
  do_check_eq(Utils.normalizeAccount("   QA1234+boo@mozilla.com	"), "QA1234+boo@mozilla.com");
  do_check_eq(Utils.normalizeAccount("QA1234+boo@mozilla.com"), "QA1234+boo@mozilla.com");
}
