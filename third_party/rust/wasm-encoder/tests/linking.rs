use anyhow::{Context, Result};
use std::fs;
use std::io::{self, Write};
use std::process::{Command, Stdio};
use wasm_encoder::*;

/// Run `wasm-objdump -x -j linking` on the given Wasm bytes.
///
/// Returns `Ok(Some(stdout))` on success, `Ok(None)` if it looks like this
/// system doesn't have WABT installed on it (so that the caller can skip the
/// current test), and `Err(e)` on some other kind of failure.
fn wabt_linking_section(wasm: &[u8]) -> Result<Option<String>> {
    let tmp = tempfile::NamedTempFile::new().context("failed to create a named temp file")?;
    fs::write(tmp.path(), wasm).context("failed to write our wasm module to a temp file")?;

    let child = match Command::new("wasm-objdump")
        .arg(tmp.path())
        .arg("-x")
        .arg("-s")
        .arg("-j")
        .arg("linking")
        .stdin(Stdio::null())
        .stderr(Stdio::null())
        .stdout(Stdio::piped())
        .spawn()
    {
        Ok(c) => c,
        Err(_) => {
            let stderr = io::stderr();
            let mut stderr = stderr.lock();
            let _ = writeln!(
                &mut stderr,
                "Warning: failed to spawn `wasm-objdump` command. Assuming WABT tools are not \
                 installed on this system and ignoring this test.",
            );
            return Ok(None);
        }
    };

    let output = child
        .wait_with_output()
        .context("failed to read wasm-objdump's stdout")?;
    let mut stdout =
        String::from_utf8(output.stdout).context("wasm-objdump did not emit UTF-8 to stdout")?;
    eprintln!(
        "====== full wasm-objdump output ======\n{}\n======================================",
        stdout
    );

    // Trim the prefix output before the linking section dump (which includes
    // the name of the tempfile, so we can't have it in here).
    let i = stdout.find(" - name: \"linking\"\n").ok_or_else(|| {
        anyhow::anyhow!("could not find linking section dump in wasm-objdump output")
    })?;
    let mut stdout = stdout.split_off(i);

    // Trim the hexdump of the custom section. While this is useful to have in
    // the `stderr` logging above when debugging tests, we don't want it
    // repeated in our test assertions.
    let i = stdout
        .find("\n\nContents of section Custom:\n")
        .ok_or_else(|| {
            anyhow::anyhow!("could not find contents of custom section in wasm-objdump output")
        })?;
    let _ = stdout.split_off(i);

    Ok(Some(stdout))
}

fn assert_wabt_linking(linking: &LinkingSection, expected_wabt_dump: &str) -> Result<()> {
    let mut module = Module::new();
    module.section(linking);
    let wasm = module.finish();

    if let Some(actual_wabt_dump) = wabt_linking_section(&wasm)? {
        assert_eq!(expected_wabt_dump.trim(), actual_wabt_dump.trim());
    }

    Ok(())
}

#[test]
fn sym_tab_function() -> Result<()> {
    let mut sym_tab = SymbolTable::new();
    sym_tab.function(0, 42, Some("func"));

    let mut linking = LinkingSection::new();
    linking.symbol_table(&sym_tab);

    assert_wabt_linking(
        &linking,
        "
 - name: \"linking\"
  - symbol table [count=1]
   - 0: F <func> func=42 [ binding=global vis=default ]
",
    )
}

#[test]
fn sym_tab_flags() -> Result<()> {
    let mut sym_tab = SymbolTable::new();
    sym_tab.function(
        SymbolTable::WASM_SYM_BINDING_WEAK | SymbolTable::WASM_SYM_VISIBILITY_HIDDEN,
        1337,
        Some("func"),
    );

    let mut linking = LinkingSection::new();
    linking.symbol_table(&sym_tab);

    assert_wabt_linking(
        &linking,
        "
 - name: \"linking\"
  - symbol table [count=1]
   - 0: F <func> func=1337 [ binding=weak vis=hidden ]
",
    )
}

#[test]
fn sym_tab_global() -> Result<()> {
    let mut sym_tab = SymbolTable::new();
    sym_tab.global(0, 42, Some("my_global"));

    let mut linking = LinkingSection::new();
    linking.symbol_table(&sym_tab);

    assert_wabt_linking(
        &linking,
        "
 - name: \"linking\"
  - symbol table [count=1]
   - 0: G <my_global> global=42 [ binding=global vis=default ]",
    )
}

#[test]
fn sym_tab_table() -> Result<()> {
    let mut sym_tab = SymbolTable::new();
    sym_tab.table(0, 42, Some("my_table"));

    let mut linking = LinkingSection::new();
    linking.symbol_table(&sym_tab);

    assert_wabt_linking(
        &linking,
        "
 - name: \"linking\"
  - symbol table [count=1]
   - 0: T <my_table> table=42 [ binding=global vis=default ]",
    )
}

#[test]
fn sym_tab_data_defined() -> Result<()> {
    let mut sym_tab = SymbolTable::new();
    sym_tab.data(
        0,
        "my_data",
        Some(DataSymbolDefinition {
            index: 42,
            offset: 1337,
            size: 1234,
        }),
    );

    let mut linking = LinkingSection::new();
    linking.symbol_table(&sym_tab);

    assert_wabt_linking(
        &linking,
        "
 - name: \"linking\"
  - symbol table [count=1]
   - 0: D <my_data> segment=42 offset=1337 size=1234 [ binding=global vis=default ]
",
    )
}

#[test]
fn sym_tab_data_undefined() -> Result<()> {
    let mut sym_tab = SymbolTable::new();
    sym_tab.data(SymbolTable::WASM_SYM_UNDEFINED, "my_data", None);

    let mut linking = LinkingSection::new();
    linking.symbol_table(&sym_tab);

    assert_wabt_linking(
        &linking,
        "
 - name: \"linking\"
  - symbol table [count=1]
   - 0: D <my_data> [ undefined binding=global vis=default ]
",
    )
}
