use lucet_module::bindings::Bindings;

pub fn bindings() -> Bindings {
    Bindings::from_str(include_str!("../bindings.json")).expect("lucet-wasi bindings.json is valid")
}

#[cfg(test)]
#[test]
fn test_bindings_parses() {
    let _ = bindings();
}
