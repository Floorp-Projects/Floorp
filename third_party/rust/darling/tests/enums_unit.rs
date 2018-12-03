//! Test expansion of enum variants which have no associated data.

#[macro_use]
extern crate darling;
extern crate syn;

#[derive(Debug, FromMetaItem)]
#[darling(rename_all="snake_case")]
enum Pattern {
    Owned,
    Immutable,
    Mutable
}

#[test]
fn expansion() {

}