use wasmparser::*;

#[test]
fn name_annotations() -> anyhow::Result<()> {
    assert_module_name("foo", r#"(module $foo)"#)?;
    assert_module_name("foo", r#"(module (@name "foo"))"#)?;
    assert_module_name("foo", r#"(module $bar (@name "foo"))"#)?;
    assert_module_name("foo bar", r#"(module $bar (@name "foo bar"))"#)?;
    Ok(())
}

fn assert_module_name(name: &str, wat: &str) -> anyhow::Result<()> {
    let wasm = wat::parse_str(wat)?;
    let mut found = false;
    for s in get_name_section(&wasm)? {
        match s? {
            Name::Module(n) => {
                assert_eq!(n.get_name()?, name);
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
                let mut map = n.get_map()?;
                let naming = map.read()?;
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
                let mut reader = n.get_function_local_reader()?;
                let section = reader.read()?;
                let mut map = section.get_map()?;
                let naming = map.read()?;
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
    for section in ModuleReader::new(wasm)? {
        let section = section?;
        match section.code {
            SectionCode::Custom {
                kind: CustomSectionKind::Name,
                ..
            } => return Ok(section.get_name_section_reader()?),
            _ => {}
        }
    }
    panic!("no name section found");
}

#[test]
fn custom_section_order() -> anyhow::Result<()> {
    let wasm = wat::parse_str(
        r#"
            (module
              (@custom "A" "aaa")
              (type $t (func))
              (@custom "B" (after func) "bbb")
              (@custom "C" (before func) "ccc")
              (@custom "D" (after last) "ddd")
              (table 10 funcref)
              (func (type $t))
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
    let mut wasm = ModuleReader::new(&wasm)?;
    let custom_kind = |name| SectionCode::Custom {
        name,
        kind: CustomSectionKind::Unknown,
    };
    assert_eq!(wasm.read()?.code, custom_kind("K"));
    assert_eq!(wasm.read()?.code, custom_kind("F"));
    assert_eq!(wasm.read()?.code, SectionCode::Type);
    assert_eq!(wasm.read()?.code, custom_kind("E"));
    assert_eq!(wasm.read()?.code, custom_kind("C"));
    assert_eq!(wasm.read()?.code, custom_kind("J"));
    assert_eq!(wasm.read()?.code, SectionCode::Function);
    assert_eq!(wasm.read()?.code, custom_kind("B"));
    assert_eq!(wasm.read()?.code, custom_kind("I"));
    assert_eq!(wasm.read()?.code, SectionCode::Table);
    assert_eq!(wasm.read()?.code, SectionCode::Code);
    assert_eq!(wasm.read()?.code, custom_kind("H"));
    assert_eq!(wasm.read()?.code, custom_kind("G"));
    assert_eq!(wasm.read()?.code, custom_kind("A"));
    assert_eq!(wasm.read()?.code, custom_kind("D"));
    assert!(wasm.eof());
    Ok(())
}
