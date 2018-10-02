use std::env;
use std::env::consts::EXE_EXTENSION;
use std::path::Path;
use std::process::Command;

#[test]
fn readme_test() {
    let rustdoc = Path::new("rustdoc").with_extension(EXE_EXTENSION);
    let readme = Path::new(file!()).parent().unwrap().parent().unwrap().join("README.md");
    let exe = env::current_exe().unwrap();
    let depdir = exe.parent().unwrap();
    let outdir = depdir.parent().unwrap();
    let extern_arg = format!("scroll={}", outdir.join("libscroll.rlib").to_string_lossy());
    let mut cmd = Command::new(rustdoc);
    cmd.args(&["--verbose", "--test", "-L"])
        .arg(&outdir)
        .arg("-L")
        .arg(&depdir)
        .arg("--extern")
        .arg(&extern_arg)
        .arg(&readme);
    println!("Running `{:?}`", cmd);
    let result = cmd.spawn()
        .expect("Failed to spawn process")
        .wait()
        .expect("Failed to run process");
    assert!(result.success(), "Failed to run rustdoc tests on README.md!");
}
