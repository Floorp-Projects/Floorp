use super::{Logger, Discard};

#[test]
fn logger_fmt_debug_sanity() {

    let root = Logger::root(Discard, o!("a" => "aa"));
    let log = root.new(o!("b" => "bb", "c" => "cc"));

    assert_eq!(format!("{:?}", log), "Logger(c, b, a)");
}

#[cfg(test)]
mod tests {
    use {Logger, Discard};
    /// ensure o! macro expands without error inside a module
    #[test]
    fn test_o_macro_expansion() {
        let _ = Logger::root(Discard, o!("a" => "aa"));
    }
}
