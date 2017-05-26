extern crate backtrace;

use std::os::raw::c_void;
use std::thread;

static LIBUNWIND: bool = cfg!(all(unix, feature = "libunwind"));
static UNIX_BACKTRACE: bool = cfg!(all(unix, feature = "unix-backtrace"));
static LIBBACKTRACE: bool = cfg!(all(unix, feature = "libbacktrace")) &&
                            !cfg!(target_os = "macos") && !cfg!(target_os = "ios");
static CORESYMBOLICATION: bool = cfg!(all(any(target_os = "macos", target_os = "ios"),
                                          feature = "coresymbolication"));
static DLADDR: bool = cfg!(all(unix, feature = "dladdr"));
static DBGHELP: bool = cfg!(all(windows, feature = "dbghelp"));
static MSVC: bool = cfg!(target_env = "msvc");

#[test]
fn smoke_test_frames() {
    frame_1(line!());
    #[inline(never)] fn frame_1(start_line: u32) { frame_2(start_line) }
    #[inline(never)] fn frame_2(start_line: u32) { frame_3(start_line) }
    #[inline(never)] fn frame_3(start_line: u32) { frame_4(start_line) }
    #[inline(never)] fn frame_4(start_line: u32) {
        let mut v = Vec::new();
        backtrace::trace(|cx| {
            v.push((cx.ip(), cx.symbol_address()));
            true
        });

        if v.len() < 5 {
            assert!(!LIBUNWIND);
            assert!(!UNIX_BACKTRACE);
            assert!(!DBGHELP);
            return
        }

        // On 32-bit windows apparently the first frame isn't our backtrace
        // frame but it's actually this frame. I'm not entirely sure why, but at
        // least it seems consistent?
        let o = if cfg!(all(windows, target_pointer_width = "32")) {1} else {0};
        // frame offset 0 is the `backtrace::trace` function, but that's generic
        assert_frame(&v, o, 1, frame_4 as usize, "frame_4",
                     "tests/smoke.rs", start_line + 6);
        assert_frame(&v, o, 2, frame_3 as usize, "frame_3", "tests/smoke.rs",
                     start_line + 3);
        assert_frame(&v, o, 3, frame_2 as usize, "frame_2", "tests/smoke.rs",
                     start_line + 2);
        assert_frame(&v, o, 4, frame_1 as usize, "frame_1", "tests/smoke.rs",
                     start_line + 1);
        assert_frame(&v, o, 5, smoke_test_frames as usize,
                     "smoke_test_frames", "", 0);
    }

    fn assert_frame(syms: &[(*mut c_void, *mut c_void)],
                    offset: usize,
                    idx: usize,
                    actual_fn_pointer: usize,
                    expected_name: &str,
                    expected_file: &str,
                    expected_line: u32) {
        if offset > idx { return }
        let (ip, sym) = syms[idx - offset];
        let ip = ip as usize;
        let sym = sym as usize;
        assert!(ip >= sym);
        assert!(sym >= actual_fn_pointer);

        // windows dbghelp is *quite* liberal (and wrong) in many of its reports
        // right now...
        if !DBGHELP {
            assert!(sym - actual_fn_pointer < 1024);
        }

        let mut resolved = 0;
        let can_resolve = DLADDR || LIBBACKTRACE || CORESYMBOLICATION || DBGHELP;

        let mut name = None;
        let mut addr = None;
        let mut line = None;
        let mut file = None;
        backtrace::resolve(ip as *mut c_void, |sym| {
            resolved += 1;
            name = sym.name().map(|v| v.to_string());
            addr = sym.addr();
            line = sym.lineno();
            file = sym.filename().map(|v| v.to_path_buf());
        });

        // dbghelp doesn't always resolve symbols right now
        match resolved {
            0 => return assert!(!can_resolve || DBGHELP),
            _ => {}
        }

        // * linux dladdr doesn't work (only consults local symbol table)
        // * windows dbghelp isn't great for GNU
        if can_resolve &&
           !(cfg!(target_os = "linux") && DLADDR) &&
           !(DBGHELP && !MSVC)
        {
            let name = name.expect("didn't find a name");
            assert!(name.contains(expected_name),
                    "didn't find `{}` in `{}`", expected_name, name);
        }

        if can_resolve {
            addr.expect("didn't find a symbol");
        }

        if (LIBBACKTRACE || CORESYMBOLICATION || (DBGHELP && MSVC)) && cfg!(debug_assertions) {
            let line = line.expect("didn't find a line number");
            let file = file.expect("didn't find a line number");
            if !expected_file.is_empty() {
                assert!(file.ends_with(expected_file),
                        "{:?} didn't end with {:?}", file, expected_file);
            }
            if expected_line != 0 {
                assert!(line == expected_line,
                        "bad line number on frame for `{}`: {} != {}",
                        expected_name, line, expected_line);
            }
        }
    }
}

#[test]
fn many_threads() {
    let threads = (0..16).map(|_| {
        thread::spawn(|| {
            for _ in 0..16 {
                backtrace::trace(|frame| {
                    backtrace::resolve(frame.ip(), |symbol| {
                        let _s = symbol.name().map(|s| s.to_string());
                    });
                    true
                });
            }
        })
    }).collect::<Vec<_>>();

    for t in threads {
        t.join().unwrap()
    }
}

#[test]
#[cfg(feature = "rustc-serialize")]
fn is_rustc_serialize() {
    extern crate rustc_serialize;

    fn is_encode<T: rustc_serialize::Encodable>() {}
    fn is_decode<T: rustc_serialize::Decodable>() {}

    is_encode::<backtrace::Backtrace>();
    is_decode::<backtrace::Backtrace>();
}

#[test]
#[cfg(feature = "serde")]
fn is_serde() {
    extern crate serde;

    fn is_serialize<T: serde::ser::Serialize>() {}
    fn is_deserialize<T: serde::de::DeserializeOwned>() {}

    is_serialize::<backtrace::Backtrace>();
    is_deserialize::<backtrace::Backtrace>();
}
