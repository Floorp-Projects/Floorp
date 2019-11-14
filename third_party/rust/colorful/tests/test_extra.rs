extern crate colorful;
extern crate core;


#[test]
fn test_extra_color() {
    use colorful::ExtraColorInterface;
    let s = "Hello world";
    assert_eq!("\x1B[38;5;16mHello world\x1B[0m".to_owned(), s.grey0().to_string());
}
