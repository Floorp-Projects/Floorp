extern crate mozrunner;

use mozrunner::runner::platform;
use std::io::Write;

fn main() {
    let (path, code) = platform::firefox_default_path()
        .map(|x| (x.to_string_lossy().into_owned(), 0))
        .unwrap_or(("Firefox binary not found".to_owned(), 1));

    let mut writer: Box<Write> = match code {
        0 => Box::new(std::io::stdout()),
        _ => Box::new(std::io::stderr())
    };
    writeln!(&mut writer, "{}", &*path).unwrap();
    std::process::exit(code);
}
