mat!(ascii_literal, r"a", "a", Some((0, 1)));

// Some crazy expressions from regular-expressions.info.
mat!(match_ranges,
     r"\b(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\b",
     "num: 255", Some((5, 8)));
mat!(match_ranges_not,
     r"\b(?:[0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\b",
     "num: 256", None);
mat!(match_float1, r"[-+]?[0-9]*\.?[0-9]+", "0.1", Some((0, 3)));
mat!(match_float2, r"[-+]?[0-9]*\.?[0-9]+", "0.1.2", Some((0, 3)));
mat!(match_float3, r"[-+]?[0-9]*\.?[0-9]+", "a1.2", Some((1, 4)));
mat!(match_float4, r"^[-+]?[0-9]*\.?[0-9]+$", "1.a", None);
mat!(match_email, r"(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,4}\b",
     "mine is jam.slam@gmail.com ", Some((8, 26)));
mat!(match_email_not, r"(?i)\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,4}\b",
     "mine is jam.slam@gmail ", None);
mat!(match_email_big, r"[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?",
     "mine is jam.slam@gmail.com ", Some((8, 26)));
mat!(match_date1,
     r"^(19|20)\d\d[- /.](0[1-9]|1[012])[- /.](0[1-9]|[12][0-9]|3[01])$",
     "1900-01-01", Some((0, 10)));
mat!(match_date2,
     r"^(19|20)\d\d[- /.](0[1-9]|1[012])[- /.](0[1-9]|[12][0-9]|3[01])$",
     "1900-00-01", None);
mat!(match_date3,
     r"^(19|20)\d\d[- /.](0[1-9]|1[012])[- /.](0[1-9]|[12][0-9]|3[01])$",
     "1900-13-01", None);

// Do some crazy dancing with the start/end assertions.
matiter!(match_start_end_empty, r"^$", "", (0, 0));
matiter!(match_start_end_empty_many_1, r"^$^$^$", "", (0, 0));
matiter!(match_start_end_empty_many_2, r"^^^$$$", "", (0, 0));
matiter!(match_start_end_empty_rev, r"$^", "", (0, 0));
matiter!(match_start_end_empty_rep, r"(?:^$)*", "a\nb\nc",
         (0, 0), (1, 1), (2, 2), (3, 3), (4, 4), (5, 5));
matiter!(match_start_end_empty_rep_rev, r"(?:$^)*", "a\nb\nc",
         (0, 0), (1, 1), (2, 2), (3, 3), (4, 4), (5, 5));

// Test negated character classes.
mat!(negclass_letters, r"[^ac]", "acx", Some((2, 3)));
mat!(negclass_letter_comma, r"[^a,]", "a,x", Some((2, 3)));
mat!(negclass_letter_space, r"[^a\s]", "a x", Some((2, 3)));
mat!(negclass_comma, r"[^,]", ",,x", Some((2, 3)));
mat!(negclass_space, r"[^\s]", " a", Some((1, 2)));
mat!(negclass_space_comma, r"[^,\s]", ", a", Some((2, 3)));
mat!(negclass_comma_space, r"[^\s,]", " ,a", Some((2, 3)));
mat!(negclass_ascii, r"[^[:alpha:]Z]", "A1", Some((1, 2)));

// Test that the DFA can handle pathological cases.
// (This should result in the DFA's cache being flushed too frequently, which
// should cause it to quit and fall back to the NFA algorithm.)
#[test]
fn dfa_handles_pathological_case() {
    fn ones_and_zeroes(count: usize) -> String {
        use rand::{Rng, thread_rng};

        let mut rng = thread_rng();
        let mut s = String::new();
        for _ in 0..count {
            if rng.gen() {
                s.push('1');
            } else {
                s.push('0');
            }
        }
        s
    }

    let re = regex!(r"[01]*1[01]{20}$");
    let text = {
        let mut pieces = ones_and_zeroes(100_000);
        pieces.push('1');
        pieces.push_str(&ones_and_zeroes(20));
        pieces
    };
    assert!(re.is_match(text!(&*text)));
}
