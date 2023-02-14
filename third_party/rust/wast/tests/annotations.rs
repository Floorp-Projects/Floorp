use wasmparser::*;

#[test]
fn name_annotations() -> anyhow::Result<()> {
    assert_module_name("foo", r#"(module $foo)"#)?;
    assert_module_name("foo", r#"(module (@name "foo"))"#)?;
    assert_module_name("foo", r#"(module $bar (@name "foo"))"#)?;
    assert_module_name("foo bar", r#"(module $bar (@name "foo bar"))"#)?;
    Ok(())
}

fn assert_module_name(expected_name: &str, wat: &str) -> anyhow::Result<()> {
    let wasm = wat::parse_str(wat)?;
    let mut found = false;
    for s in get_name_section(&wasm)? {
        match s? {
            Name::Module { name, .. } => {
                assert_eq!(name, expected_name);
                found = true;
            }
            _ => {}
        }
    }
    assert!(found);
    Ok(())
}

#[test]
fn func_annotations() -> anyhow::Result<()> {
    assert_func_name("foo", r#"(module (func $foo))"#)?;
    assert_func_name("foo", r#"(module (func (@name "foo")))"#)?;
    assert_func_name("foo", r#"(module (func $bar (@name "foo")))"#)?;
    assert_func_name("foo bar", r#"(module (func $bar (@name "foo bar")))"#)?;
    Ok(())
}

fn assert_func_name(name: &str, wat: &str) -> anyhow::Result<()> {
    let wasm = wat::parse_str(wat)?;
    let mut found = false;
    for s in get_name_section(&wasm)? {
        match s? {
            Name::Function(n) => {
                let naming = n.into_iter().next().unwrap()?;
                assert_eq!(naming.index, 0);
                assert_eq!(naming.name, name);
                found = true;
            }
            _ => {}
        }
    }
    assert!(found);
    Ok(())
}

#[test]
fn local_annotations() -> anyhow::Result<()> {
    assert_local_name("foo", r#"(module (func (param $foo i32)))"#)?;
    assert_local_name("foo", r#"(module (func (local $foo i32)))"#)?;
    assert_local_name("foo", r#"(module (func (param (@name "foo") i32)))"#)?;
    assert_local_name("foo", r#"(module (func (local (@name "foo") i32)))"#)?;
    assert_local_name("foo", r#"(module (func (param $bar (@name "foo") i32)))"#)?;
    assert_local_name("foo", r#"(module (func (local $bar (@name "foo") i32)))"#)?;
    assert_local_name(
        "foo bar",
        r#"(module (func (param $bar (@name "foo bar") i32)))"#,
    )?;
    assert_local_name(
        "foo bar",
        r#"(module (func (local $bar (@name "foo bar") i32)))"#,
    )?;
    Ok(())
}

fn assert_local_name(name: &str, wat: &str) -> anyhow::Result<()> {
    let wasm = wat::parse_str(wat)?;
    let mut found = false;
    for s in get_name_section(&wasm)? {
        match s? {
            Name::Local(n) => {
                let naming = n
                    .into_iter()
                    .next()
                    .unwrap()?
                    .names
                    .into_iter()
                    .next()
                    .unwrap()?;
                assert_eq!(naming.index, 0);
                assert_eq!(naming.name, name);
                found = true;
            }
            _ => {}
        }
    }
    assert!(found);
    Ok(())
}

fn get_name_section(wasm: &[u8]) -> anyhow::Result<NameSectionReader<'_>> {
    for payload in Parser::new(0).parse_all(&wasm) {
        if let Payload::CustomSection(c) = payload? {
            if c.name() == "name" {
                return Ok(NameSectionReader::new(c.data(), c.data_offset()));
            }
        }
    }
    panic!("no name section found");
}

#[test]
fn custom_section_order() -> anyhow::Result<()> {
    let bytes = wat::parse_str(
        r#"
            (module
              (@custom "A" "aaa")
              (type (func))
              (@custom "B" (after func) "bbb")
              (@custom "C" (before func) "ccc")
              (@custom "D" (after last) "ddd")
              (table 10 funcref)
              (func (type 0))
              (@custom "E" (after import) "eee")
              (@custom "F" (before type) "fff")
              (@custom "G" (after data) "ggg")
              (@custom "H" (after code) "hhh")
              (@custom "I" (after func) "iii")
              (@custom "J" (before func) "jjj")
              (@custom "K" (before first) "kkk")
            )
        "#,
    )?;
    macro_rules! assert_matches {
        ($a:expr, $b:pat $(if $cond:expr)? $(,)?) => {
            match &$a {
                $b $(if $cond)? => {}
                a => panic!("`{:?}` doesn't match `{}`", a, stringify!($b)),
            }
        };
    }
    let wasm = Parser::new(0)
        .parse_all(&bytes)
        .collect::<Result<Vec<_>>>()?;
    assert_matches!(wasm[0], Payload::Version { .. });
    assert_matches!(
        wasm[1],
        Payload::CustomSection(c) if c.name() == "K"
    );
    assert_matches!(
        wasm[2],
        Payload::CustomSection(c) if c.name() == "F"
    );
    assert_matches!(wasm[3], Payload::TypeSection(_));
    assert_matches!(
        wasm[4],
        Payload::CustomSection(c) if c.name() == "E"
    );
    assert_matches!(
        wasm[5],
        Payload::CustomSection(c) if c.name() == "C"
    );
    assert_matches!(
        wasm[6],
        Payload::CustomSection(c) if c.name() == "J"
    );
    assert_matches!(wasm[7], Payload::FunctionSection(_));
    assert_matches!(
        wasm[8],
        Payload::CustomSection(c) if c.name() == "B"
    );
    assert_matches!(
        wasm[9],
        Payload::CustomSection(c) if c.name() == "I"
    );
    assert_matches!(wasm[10], Payload::TableSection(_));
    assert_matches!(wasm[11], Payload::CodeSectionStart { .. });
    assert_matches!(wasm[12], Payload::CodeSectionEntry { .. });
    assert_matches!(
        wasm[13],
        Payload::CustomSection(c) if c.name() == "H"
    );
    assert_matches!(
        wasm[14],
        Payload::CustomSection(c) if c.name() == "G"
    );
    assert_matches!(
        wasm[15],
        Payload::CustomSection(c) if c.name() == "A"
    );
    assert_matches!(
        wasm[16],
        Payload::CustomSection(c) if c.name() == "D"
    );

    match &wasm[17] {
        Payload::End(x) if *x == bytes.len() => {}
        p => panic!("`{:?}` doesn't match expected length of {}", p, bytes.len()),
    }

    Ok(())
}
