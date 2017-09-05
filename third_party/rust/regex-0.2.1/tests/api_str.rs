// These tests don't really make sense with the bytes API, so we only test them
// on the Unicode API.

#[test]
fn empty_match_unicode_find_iter() {
    // Tests that we still yield byte ranges at valid UTF-8 sequence boundaries
    // even when we're susceptible to empty width matches.
    let re = regex!(r".*?");
    assert_eq!(vec![(0, 0), (3, 3), (4, 4), (7, 7), (8, 8)],
               findall!(re, "Ⅰ1Ⅱ2"));
}

#[test]
fn empty_match_unicode_captures_iter() {
    // Same as empty_match_unicode_find_iter, but tests capture iteration.
    let re = regex!(r".*?");
    let ms: Vec<_> = re.captures_iter(text!("Ⅰ1Ⅱ2"))
                       .map(|c| c.get(0).unwrap())
                       .map(|m| (m.start(), m.end()))
                       .collect();
    assert_eq!(vec![(0, 0), (3, 3), (4, 4), (7, 7), (8, 8)], ms);
}
