use std::env;

fn rmain() -> Option<()> {
    // Missing? No idea what's going on.
    let target_os = env::var("CARGO_CFG_TARGET_OS").ok()?;
    if target_os == "windows" {
        let mut builder = cc::Build::new();
        builder.file("c/windows.c");
        builder.compile("info");
    }

    Some(())
}

fn main() {
    let _ = rmain();
}
