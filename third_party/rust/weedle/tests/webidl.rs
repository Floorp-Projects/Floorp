extern crate weedle;

use std::fs;
use std::io::Read;

fn read_file(path: &str) -> String {
    let mut file = fs::File::open(path).unwrap();
    let mut file_content = String::new();
    file.read_to_string(&mut file_content).unwrap();
    file_content
}

#[test]
pub fn should_parse_dom_webidl() {
    let content = read_file("./tests/defs/dom.webidl");
    let parsed = weedle::parse(&content).unwrap();

    assert_eq!(parsed.len(), 62);
}

#[test]
fn should_parse_html_webidl() {
    let content = read_file("./tests/defs/html.webidl");
    let parsed = weedle::parse(&content).unwrap();

    assert_eq!(parsed.len(), 325);
}
