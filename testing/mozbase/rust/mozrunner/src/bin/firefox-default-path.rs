/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate mozrunner;

use mozrunner::runner::platform;
use std::io::Write;

fn main() {
    let (path, code) = platform::firefox_default_path()
        .map(|x| (x.to_string_lossy().into_owned(), 0))
        .unwrap_or(("Firefox binary not found".to_owned(), 1));

    let mut writer: Box<dyn Write> = match code {
        0 => Box::new(std::io::stdout()),
        _ => Box::new(std::io::stderr()),
    };
    writeln!(&mut writer, "{}", &*path).unwrap();
    std::process::exit(code);
}
