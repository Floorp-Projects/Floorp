extern crate regex;

use regex::Regex;

fn main() {
    let re = Regex::new("^(aba|a)c*(b|$)").unwrap();
    println!("{:?}", re.find("abaca"));
    println!("{:?}", re.captures("abaca"));
}
