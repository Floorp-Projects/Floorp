// See: https://github.com/rust-lang/regex/issues/48
#[test]
fn invalid_regexes_no_crash() {
    assert!(regex_new!("(*)").is_err());
    assert!(regex_new!("(?:?)").is_err());
    assert!(regex_new!("(?)").is_err());
    assert!(regex_new!("*").is_err());
}

// See: https://github.com/rust-lang/regex/issues/98
#[test]
fn regression_many_repeat_stack_overflow() {
    let re = regex!("^.{1,2500}");
    assert_eq!(vec![(0, 1)], findall!(re, "a"));
}

// See: https://github.com/rust-lang/regex/issues/75
mat!(regression_unsorted_binary_search_1, r"(?i)[a_]+", "A_", Some((0, 2)));
mat!(regression_unsorted_binary_search_2, r"(?i)[A_]+", "a_", Some((0, 2)));

// See: https://github.com/rust-lang/regex/issues/99
mat!(regression_negated_char_class_1, r"(?i)[^x]", "x", None);
mat!(regression_negated_char_class_2, r"(?i)[^x]", "X", None);

// See: https://github.com/rust-lang/regex/issues/101
mat!(regression_ascii_word_underscore, r"[[:word:]]", "_", Some((0, 1)));

// See: https://github.com/rust-lang/regex/issues/129
#[test]
fn regression_captures_rep() {
    let re = regex!(r"([a-f]){2}(?P<foo>[x-z])");
    let caps = re.captures(text!("abx")).unwrap();
    assert_eq!(match_text!(caps.name("foo").unwrap()), text!("x"));
}

// See: https://github.com/rust-lang/regex/issues/153
mat!(regression_alt_in_alt1, r"ab?|$", "az", Some((0, 1)));
mat!(regression_alt_in_alt2, r"^(.*?)(\n|\r\n?|$)", "ab\rcd", Some((0, 3)));

// See: https://github.com/rust-lang/regex/issues/169
mat!(regression_leftmost_first_prefix, r"z*azb", "azb", Some((0, 3)));

// See: https://github.com/rust-lang/regex/issues/76
mat!(uni_case_lower_nocase_flag, r"(?i)\p{Ll}+", "ΛΘΓΔα", Some((0, 10)));

// See: https://github.com/rust-lang/regex/issues/191
mat!(many_alternates, r"1|2|3|4|5|6|7|8|9|10|int", "int", Some((0, 3)));

// burntsushi was bad and didn't create an issue for this bug.
mat!(anchored_prefix1, r"^a\S", "a ", None);
mat!(anchored_prefix2, r"^a\S", "foo boo a ", None);
mat!(anchored_prefix3, r"^-[a-z]", "r-f", None);

// See: https://github.com/rust-lang/regex/issues/204
split!(split_on_word_boundary, r"\b", r"Should this (work?)",
       &[t!(""), t!("Should"), t!(" "), t!("this"),
         t!(" ("), t!("work"), t!("?)")]);
matiter!(word_boundary_dfa, r"\b", "a b c",
         (0, 0), (1, 1), (2, 2), (3, 3), (4, 4), (5, 5));

// See: https://github.com/rust-lang/regex/issues/268
matiter!(partial_anchor, r"^a|b", "ba", (0, 1));

// See: https://github.com/rust-lang/regex/issues/280
ismatch!(partial_anchor_alternate_begin, r"^a|z", "yyyyya", false);
ismatch!(partial_anchor_alternate_end, r"a$|z", "ayyyyy", false);

// See: https://github.com/rust-lang/regex/issues/289
mat!(lits_unambiguous1, r"(ABC|CDA|BC)X", "CDAX", Some((0, 4)));

// See: https://github.com/rust-lang/regex/issues/291
mat!(lits_unambiguous2, r"((IMG|CAM|MG|MB2)_|(DSCN|CIMG))(?P<n>[0-9]+)$",
     "CIMG2341", Some((0, 8)), Some((0, 4)), None, Some((0, 4)), Some((4, 8)));

// See: https://github.com/rust-lang/regex/issues/271
mat!(endl_or_wb, r"(?m:$)|(?-u:\b)", "\u{6084e}", Some((4, 4)));
mat!(zero_or_end, r"(?i-u:\x00)|$", "\u{e682f}", Some((4, 4)));
mat!(y_or_endl, r"(?i-u:y)|(?m:$)", "\u{b4331}", Some((4, 4)));
mat!(wb_start_x, r"(?u:\b)^(?-u:X)", "X", Some((0, 1)));

// See: https://github.com/rust-lang/regex/issues/321
ismatch!(strange_anchor_non_complete_prefix, r"a^{2}", "", false);
ismatch!(strange_anchor_non_complete_suffix, r"${2}a", "", false);

// See: https://github.com/rust-lang/regex/issues/334
mat!(captures_after_dfa_premature_end, r"a(b*(X|$))?", "abcbX",
     Some((0, 1)), None, None);

// See: https://github.com/rust-lang/regex/issues/437
ismatch!(
    literal_panic,
    r"typename type\-parameter\-\d+\-\d+::.+",
    "test",
    false);
