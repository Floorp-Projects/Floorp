use super::intern;

#[test]
fn basic() {
    let i = intern("hello");
    let j = intern("world");
    assert!(i != j);
    assert_eq!(intern("hello"), i);
    assert_eq!(i.to_string(), "hello");
    assert_eq!(j.to_string(), "world");
}

#[test]
fn debug() {
    let i = intern("hello");
    assert_eq!(format!("{:?}", i), "\"hello\"");
}

#[test]
fn display() {
    let i = intern("hello");
    assert_eq!(format!("{}", i), "hello");
}
