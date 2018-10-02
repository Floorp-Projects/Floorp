use std::process;

pub fn compare(args: Vec<&str>) {
    let apple = process::Command::new("/Library/Developer/CommandLineTools/usr/bin/dyldinfo")
        .args(&args)
        .output()
        .expect("run Apple dyldinfo");

    let goblin = process::Command::new("cargo")
        .arg("run")
        .arg("--quiet")
        .arg("--example")
        .arg("dyldinfo")
        .arg("--")
        .args(&args)
        .output()
        .expect("run cargo dyldinfo");

    if apple.stdout.as_slice() != goblin.stdout.as_slice() {
        println!("dyldinfo calls disagree!");
        println!("Apple dyldinfo {:?} output:\n{}", &args, String::from_utf8_lossy(&apple.stdout));
        println!("---");
        println!("cargo dyldinfo {:?} output:\n{}", &args, String::from_utf8_lossy(&goblin.stdout));
        panic!("Apple dyldinfo and cargo dyldinfo differed (args: {:?})", args);
    }
}

#[cfg(target_os="macos")]
#[test]
fn compare_binds() {
    compare(vec!["-bind", "/Library/Developer/CommandLineTools/usr/bin/dyldinfo"]);
    compare(vec!["-bind", "/Library/Developer/CommandLineTools/usr/bin/clang"]);
    compare(vec!["-bind", "/usr/bin/tmutil"]);
}

#[cfg(target_os="macos")]
#[test]
fn compare_lazy_binds() {
    compare(vec!["-lazy_bind", "/Library/Developer/CommandLineTools/usr/bin/dyldinfo"]);
    compare(vec!["-lazy_bind", "/Library/Developer/CommandLineTools/usr/bin/clang"]);
    compare(vec!["-lazy_bind", "/usr/bin/tmutil"]);
}

#[cfg(target_os="macos")]
#[test]
fn compare_combined_options() {
    compare(vec!["-lazy_bind", "-bind", "/Library/Developer/CommandLineTools/usr/bin/dyldinfo"]);
}

#[cfg(not(target_os="macos"))]
#[test]
fn skipped_on_this_platform() {
    // this test does nothing on other platforms
}
