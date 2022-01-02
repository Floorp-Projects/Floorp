use cstr::cstr;

fn main() {
    let _foo = cstr!(1);
    let _foo = cstr!(("a"));
    let _foo = cstr!(&1);
}
