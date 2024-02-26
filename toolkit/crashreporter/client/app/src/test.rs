/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Tests here mostly interact with the [test UI](crate::ui::test). As such, most tests read a bit
//! more like integration tests than unit tests, testing the behavior of the application as a
//! whole.

use super::*;
use crate::config::{test::MINIDUMP_PRUNE_SAVE_COUNT, Config};
use crate::settings::Settings;
use crate::std::{
    ffi::OsString,
    fs::{MockFS, MockFiles},
    io::ErrorKind,
    mock,
    process::Command,
    sync::{
        atomic::{AtomicUsize, Ordering::Relaxed},
        Arc,
    },
};
use crate::ui::{self, test::model, ui_impl::Interact};

/// A simple thread-safe counter which can be used in tests to mark that certain code paths were
/// hit.
#[derive(Clone, Default)]
struct Counter(Arc<AtomicUsize>);

impl Counter {
    /// Create a new zero counter.
    pub fn new() -> Self {
        Self::default()
    }

    /// Increment the counter.
    pub fn inc(&self) {
        self.0.fetch_add(1, Relaxed);
    }

    /// Get the current count.
    pub fn count(&self) -> usize {
        self.0.load(Relaxed)
    }

    /// Assert that the current count is 1.
    pub fn assert_one(&self) {
        assert_eq!(self.count(), 1);
    }
}

/// Fluent wraps arguments with the unicode BiDi characters.
struct FluentArg<T>(T);

impl<T: std::fmt::Display> std::fmt::Display for FluentArg<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        use crate::std::fmt::Write;
        f.write_char('\u{2068}')?;
        self.0.fmt(f)?;
        f.write_char('\u{2069}')
    }
}

/// Run a gui and interaction on separate threads.
fn gui_interact<G, I, R>(gui: G, interact: I) -> R
where
    G: FnOnce() -> R,
    I: FnOnce(Interact) + Send + 'static,
{
    let i = Interact::hook();
    let handle = {
        let i = i.clone();
        ::std::thread::spawn(move || {
            i.wait_for_ready();
            interact(i);
        })
    };
    let ret = gui();
    // In case the gui failed before launching.
    i.cancel();
    handle.join().unwrap();
    ret
}

const MOCK_MINIDUMP_EXTRA: &str = r#"{
                            "Vendor": "FooCorp",
                            "ProductName": "Bar",
                            "ReleaseChannel": "release",
                            "BuildID": "1234",
                            "StackTraces": {
                                "status": "OK"
                            },
                            "Version": "100.0",
                            "ServerURL": "https://reports.example.com",
                            "TelemetryServerURL": "https://telemetry.example.com",
                            "TelemetryClientId": "telemetry_client",
                            "TelemetrySessionId": "telemetry_session",
                            "SomeNestedJson": { "foo": "bar" },
                            "URL": "https://url.example.com"
                        }"#;

// Actual content doesn't matter, aside from the hash that is generated.
const MOCK_MINIDUMP_FILE: &[u8] = &[1, 2, 3, 4];
const MOCK_MINIDUMP_SHA256: &str =
    "9f64a747e1b97f131fabb6b447296c9b6f0201e79fb3c5356e6c77e89b6a806a";
macro_rules! current_date {
    () => {
        "2004-11-09"
    };
}
const MOCK_CURRENT_DATE: &str = current_date!();
const MOCK_CURRENT_TIME: &str = concat!(current_date!(), "T12:34:56Z");
const MOCK_PING_UUID: uuid::Uuid = uuid::Uuid::nil();
const MOCK_REMOTE_CRASH_ID: &str = "8cbb847c-def2-4f68-be9e-000000000000";

fn current_datetime() -> time::OffsetDateTime {
    time::OffsetDateTime::parse(
        MOCK_CURRENT_TIME,
        &time::format_description::well_known::Rfc3339,
    )
    .unwrap()
}

fn current_unix_time() -> i64 {
    current_datetime().unix_timestamp()
}

fn current_system_time() -> ::std::time::SystemTime {
    current_datetime().into()
}

/// A basic configuration which populates some necessary/useful fields.
fn test_config() -> Config {
    let mut cfg = Config::default();
    cfg.data_dir = Some("data_dir".into());
    cfg.events_dir = Some("events_dir".into());
    cfg.ping_dir = Some("ping_dir".into());
    cfg.dump_file = Some("minidump.dmp".into());
    cfg.strings = lang::LanguageInfo::default().load_strings().ok();
    cfg
}

/// A test fixture to make configuration, mocking, and assertions easier.
struct GuiTest {
    /// The configuration used in the test. Initialized to [`test_config`].
    pub config: Config,
    /// The mock builder used in the test, initialized with a basic set of mocked values to ensure
    /// most things will work out of the box.
    pub mock: mock::Builder,
    /// The mocked filesystem, which can be used for mock setup and assertions after completion.
    pub files: MockFiles,
}

impl GuiTest {
    /// Create a new GuiTest with enough configured for the application to run
    pub fn new() -> Self {
        // Create a default set of files which allow successful operation.
        let mock_files = MockFiles::new();
        mock_files
            .add_file_result(
                "minidump.dmp",
                Ok(MOCK_MINIDUMP_FILE.into()),
                current_system_time(),
            )
            .add_file_result(
                "minidump.extra",
                Ok(MOCK_MINIDUMP_EXTRA.into()),
                current_system_time(),
            );

        // Create a default mock environment which allows successful operation.
        let mut mock = mock::builder();
        mock.set(
            Command::mock("work_dir/minidump-analyzer"),
            Box::new(|_| Ok(crate::std::process::success_output())),
        )
        .set(
            Command::mock("work_dir/pingsender"),
            Box::new(|_| Ok(crate::std::process::success_output())),
        )
        .set(
            Command::mock("curl"),
            Box::new(|_| {
                let mut output = crate::std::process::success_output();
                output.stdout = format!("CrashID={MOCK_REMOTE_CRASH_ID}").into();
                Ok(output)
            }),
        )
        .set(MockFS, mock_files.clone())
        .set(
            crate::std::env::MockCurrentExe,
            "work_dir/crashreporter".into(),
        )
        .set(crate::std::time::MockCurrentTime, current_system_time())
        .set(mock::MockHook::new("ping_uuid"), MOCK_PING_UUID);

        GuiTest {
            config: test_config(),
            mock,
            files: mock_files,
        }
    }

    /// Run the test as configured, using the given function to interact with the GUI.
    ///
    /// Returns the final result of the application logic.
    pub fn try_run<F: FnOnce(Interact) + Send + 'static>(
        &mut self,
        interact: F,
    ) -> anyhow::Result<bool> {
        let GuiTest {
            ref mut config,
            ref mut mock,
            ..
        } = self;
        let mut config = Arc::new(std::mem::take(config));

        // Run the mock environment.
        mock.run(move || gui_interact(move || try_run(&mut config), interact))
    }

    /// Run the test as configured, using the given function to interact with the GUI.
    ///
    /// Panics if the application logic returns an error (which would normally be displayed to the
    /// user).
    pub fn run<F: FnOnce(Interact) + Send + 'static>(&mut self, interact: F) {
        if let Err(e) = self.try_run(interact) {
            panic!(
                "gui failure:{}",
                e.chain().map(|e| format!("\n  {e}")).collect::<String>()
            );
        }
    }

    /// Get the file assertion helper.
    pub fn assert_files(&self) -> AssertFiles {
        AssertFiles {
            data_dir: "data_dir".into(),
            events_dir: "events_dir".into(),
            inner: self.files.assert_files(),
        }
    }
}

/// A wrapper around the mock [`AssertFiles`](crate::std::fs::AssertFiles).
///
/// This implements higher-level assertions common across tests, but also supports the lower-level
/// assertions (though those return the [`AssertFiles`](crate::std::fs::AssertFiles) reference so
/// higher-level assertions must be chained first).
struct AssertFiles {
    data_dir: String,
    events_dir: String,
    inner: std::fs::AssertFiles,
}

impl AssertFiles {
    fn data(&self, rest: &str) -> String {
        format!("{}/{rest}", &self.data_dir)
    }

    fn events(&self, rest: &str) -> String {
        format!("{}/{rest}", &self.events_dir)
    }

    /// Set the data dir if not the default.
    pub fn set_data_dir<S: ToString>(&mut self, data_dir: S) -> &mut Self {
        let data_dir = data_dir.to_string();
        // Data dir should be relative to root.
        self.data_dir = data_dir.trim_start_matches('/').to_string();
        self
    }

    /// Ignore the generated log file.
    pub fn ignore_log(&mut self) -> &mut Self {
        self.inner.ignore(self.data("submit.log"));
        self
    }

    /// Assert that the crash report was submitted according to the filesystem.
    pub fn submitted(&mut self) -> &mut Self {
        self.inner.check(
            self.data(&format!("submitted/{MOCK_REMOTE_CRASH_ID}.txt")),
            format!("Crash ID: {}\n", FluentArg(MOCK_REMOTE_CRASH_ID)),
        );
        self
    }

    /// Assert that the given settings where saved.
    pub fn saved_settings(&mut self, settings: Settings) -> &mut Self {
        self.inner.check(
            self.data("crashreporter_settings.json"),
            settings.to_string(),
        );
        self
    }

    /// Assert that a crash is pending according to the filesystem.
    pub fn pending(&mut self) -> &mut Self {
        let dmp = self.data("pending/minidump.dmp");
        self.inner
            .check(self.data("pending/minidump.extra"), MOCK_MINIDUMP_EXTRA)
            .check_bytes(dmp, MOCK_MINIDUMP_FILE);
        self
    }

    /// Assert that a crash ping was sent according to the filesystem.
    pub fn ping(&mut self) -> &mut Self {
        self.inner.check(
            format!("ping_dir/{MOCK_PING_UUID}.json"),
            serde_json::json! {{
                "type": "crash",
                "id": MOCK_PING_UUID,
                "version": 4,
                "creation_date": MOCK_CURRENT_TIME,
                "client_id": "telemetry_client",
                "payload": {
                    "sessionId": "telemetry_session",
                    "version": 1,
                    "crashDate": MOCK_CURRENT_DATE,
                    "crashTime": MOCK_CURRENT_TIME,
                    "hasCrashEnvironment": true,
                    "crashId": "minidump",
                    "minidumpSha256Hash": MOCK_MINIDUMP_SHA256,
                    "processType": "main",
                    "stackTraces": {
                        "status": "OK"
                    },
                    "metadata": {
                        "BuildID": "1234",
                        "ProductName": "Bar",
                        "ReleaseChannel": "release",
                    }
                },
                "application": {
                    "vendor": "FooCorp",
                    "name": "Bar",
                    "buildId": "1234",
                    "displayVersion": "",
                    "platformVersion": "",
                    "version": "100.0",
                    "channel": "release"
                }
            }}
            .to_string(),
        );
        self
    }

    /// Assert that a crash submission event was written with the given submission status.
    pub fn submission_event(&mut self, success: bool) -> &mut Self {
        self.inner.check(
            self.events("minidump-submission"),
            format!(
                "crash.submission.1\n\
                {}\n\
                minidump\n\
                {success}\n\
                {}",
                current_unix_time(),
                if success { MOCK_REMOTE_CRASH_ID } else { "" }
            ),
        );
        self
    }
}

impl std::ops::Deref for AssertFiles {
    type Target = std::fs::AssertFiles;
    fn deref(&self) -> &Self::Target {
        &self.inner
    }
}

impl std::ops::DerefMut for AssertFiles {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.inner
    }
}

#[test]
fn error_dialog() {
    gui_interact(
        || {
            let cfg = Config::default();
            ui::error_dialog(&cfg, "an error occurred")
        },
        |interact| {
            interact.element("close", |_style, b: &model::Button| b.click.fire(&()));
        },
    );
}

#[test]
fn no_dump_file() {
    let mut cfg = Arc::new(Config::default());
    {
        let cfg = Arc::get_mut(&mut cfg).unwrap();
        cfg.strings = lang::LanguageInfo::default().load_strings().ok();
    }
    assert!(try_run(&mut cfg).is_err());
    Arc::get_mut(&mut cfg).unwrap().auto_submit = true;
    assert!(try_run(&mut cfg).is_ok());
}

#[test]
fn minidump_analyzer_error() {
    mock::builder()
        .set(
            Command::mock("work_dir/minidump-analyzer"),
            Box::new(|_| Err(ErrorKind::NotFound.into())),
        )
        .set(
            crate::std::env::MockCurrentExe,
            "work_dir/crashreporter".into(),
        )
        .run(|| {
            let cfg = test_config();
            assert!(try_run(&mut Arc::new(cfg)).is_err());
        });
}

#[test]
fn no_extra_file() {
    mock::builder()
        .set(
            Command::mock("work_dir/minidump-analyzer"),
            Box::new(|_| Ok(crate::std::process::success_output())),
        )
        .set(
            crate::std::env::MockCurrentExe,
            "work_dir/crashreporter".into(),
        )
        .set(MockFS, {
            let files = MockFiles::new();
            files.add_file_result(
                "minidump.extra",
                Err(ErrorKind::NotFound.into()),
                ::std::time::SystemTime::UNIX_EPOCH,
            );
            files
        })
        .run(|| {
            let cfg = test_config();
            assert!(try_run(&mut Arc::new(cfg)).is_err());
        });
}

#[test]
fn auto_submit() {
    let mut test = GuiTest::new();
    test.config.auto_submit = true;
    // auto_submit should not do any GUI things, including creating the crashreporter_settings.json
    // file.
    test.mock.run(|| {
        assert!(try_run(&mut Arc::new(std::mem::take(&mut test.config))).is_ok());
    });
    test.assert_files().ignore_log().submitted().pending();
}

#[test]
fn restart() {
    let mut test = GuiTest::new();
    test.config.restart_command = Some("my_process".into());
    test.config.restart_args = vec!["a".into(), "b".into()];
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock.set(
        Command::mock("my_process"),
        Box::new(move |cmd| {
            assert_eq!(cmd.args, &["a", "b"]);
            mock_ran_process.inc();
            Ok(crate::std::process::success_output())
        }),
    );
    test.run(|interact| {
        interact.element("restart", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted()
        .pending();
    ran_process.assert_one();
}

#[test]
fn no_restart_with_windows_error_reporting() {
    let mut test = GuiTest::new();
    test.config.restart_command = Some("my_process".into());
    test.config.restart_args = vec!["a".into(), "b".into()];
    // Add the "WindowsErrorReporting" key to the extra file
    const MINIDUMP_EXTRA_CONTENTS: &str = r#"{
                            "Vendor": "FooCorp",
                            "ProductName": "Bar",
                            "ReleaseChannel": "release",
                            "BuildID": "1234",
                            "StackTraces": {
                                "status": "OK"
                            },
                            "Version": "100.0",
                            "ServerURL": "https://reports.example.com",
                            "TelemetryServerURL": "https://telemetry.example.com",
                            "TelemetryClientId": "telemetry_client",
                            "TelemetrySessionId": "telemetry_session",
                            "SomeNestedJson": { "foo": "bar" },
                            "URL": "https://url.example.com",
                            "WindowsErrorReporting": 1
                        }"#;
    test.files = {
        let mock_files = MockFiles::new();
        mock_files
            .add_file_result(
                "minidump.dmp",
                Ok(MOCK_MINIDUMP_FILE.into()),
                current_system_time(),
            )
            .add_file_result(
                "minidump.extra",
                Ok(MINIDUMP_EXTRA_CONTENTS.into()),
                current_system_time(),
            );
        test.mock.set(MockFS, mock_files.clone());
        mock_files
    };
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock.set(
        Command::mock("my_process"),
        Box::new(move |cmd| {
            assert_eq!(cmd.args, &["a", "b"]);
            mock_ran_process.inc();
            Ok(crate::std::process::success_output())
        }),
    );
    test.run(|interact| {
        interact.element("restart", |style, b: &model::Button| {
            // Check that the button is hidden, and invoke the click anyway to ensure the process
            // isn't restarted (the window will still be closed).
            assert_eq!(style.visible.get(), false);
            b.click.fire(&())
        });
    });
    let mut assert_files = test.assert_files();
    assert_files
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted();
    {
        let dmp = assert_files.data("pending/minidump.dmp");
        let extra = assert_files.data("pending/minidump.extra");
        assert_files
            .check(extra, MINIDUMP_EXTRA_CONTENTS)
            .check_bytes(dmp, MOCK_MINIDUMP_FILE);
    }

    assert_eq!(ran_process.count(), 0);
}

#[test]
fn quit() {
    let mut test = GuiTest::new();
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted()
        .pending();
}

#[test]
fn delete_dump() {
    let mut test = GuiTest::new();
    test.config.delete_dump = true;
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted();
}

#[test]
fn no_submit() {
    let mut test = GuiTest::new();
    test.files.add_dir("data_dir").add_file(
        "data_dir/crashreporter_settings.json",
        Settings {
            submit_report: true,
            include_url: true,
        }
        .to_string(),
    );
    test.run(|interact| {
        interact.element("send", |_style, c: &model::Checkbox| {
            assert!(c.checked.get())
        });
        interact.element("include-url", |_style, c: &model::Checkbox| {
            assert!(c.checked.get())
        });
        interact.element("send", |_style, c: &model::Checkbox| c.checked.set(false));
        interact.element("include-url", |_style, c: &model::Checkbox| {
            c.checked.set(false)
        });
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .ignore_log()
        .saved_settings(Settings {
            submit_report: false,
            include_url: false,
        })
        .pending();
}

#[test]
fn ping_and_event_files() {
    let mut test = GuiTest::new();
    test.files
        .add_dir("ping_dir")
        .add_dir("events_dir")
        .add_file(
            "events_dir/minidump",
            "1\n\
         12:34:56\n\
         e0423878-8d59-4452-b82e-cad9c846836e\n\
         {\"foo\":\"bar\"}",
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted()
        .pending()
        .submission_event(true)
        .ping()
        .check(
            "events_dir/minidump",
            format!(
                "1\n\
                12:34:56\n\
                e0423878-8d59-4452-b82e-cad9c846836e\n\
                {}",
                serde_json::json! {{
                    "foo": "bar",
                    "MinidumpSha256Hash": MOCK_MINIDUMP_SHA256,
                    "CrashPingUUID": MOCK_PING_UUID,
                    "StackTraces": { "status": "OK" }
                }}
            ),
        );
}

#[test]
fn pingsender_failure() {
    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("work_dir/pingsender"),
        Box::new(|_| Err(ErrorKind::NotFound.into())),
    );
    test.files
        .add_dir("ping_dir")
        .add_dir("events_dir")
        .add_file(
            "events_dir/minidump",
            "1\n\
         12:34:56\n\
         e0423878-8d59-4452-b82e-cad9c846836e\n\
         {\"foo\":\"bar\"}",
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted()
        .pending()
        .submission_event(true)
        .ping()
        .check(
            "events_dir/minidump",
            format!(
                "1\n\
                12:34:56\n\
                e0423878-8d59-4452-b82e-cad9c846836e\n\
                {}",
                serde_json::json! {{
                    "foo": "bar",
                    "MinidumpSha256Hash": MOCK_MINIDUMP_SHA256,
                    // No crash ping UUID since pingsender fails
                    "StackTraces": { "status": "OK" }
                }}
            ),
        );
}

#[test]
fn eol_version() {
    let mut test = GuiTest::new();
    test.files
        .add_dir("data_dir")
        .add_file("data_dir/EndOfLife100.0", "");
    // Should fail before opening the gui
    let result = test.try_run(|_| ());
    assert_eq!(
        result.expect_err("should fail on EOL version").to_string(),
        "Version end of life: crash reports are no longer accepted."
    );
    test.assert_files()
        .ignore_log()
        .pending()
        .ignore("data_dir/EndOfLife100.0");
}

#[test]
fn details_window() {
    let mut test = GuiTest::new();
    test.run(|interact| {
        let details_visible = || {
            interact.window("crash-details-window", |style, _w: &model::Window| {
                style.visible.get()
            })
        };
        assert_eq!(details_visible(), false);
        interact.element("details", |_style, b: &model::Button| b.click.fire(&()));
        assert_eq!(details_visible(), true);
        let details_text = loop {
            let v = interact.element("details-text", |_style, t: &model::TextBox| t.content.get());
            if v == "Loading…" {
                // Wait for the details to be populated.
                std::thread::sleep(std::time::Duration::from_millis(50));
                continue;
            } else {
                break v;
            }
        };
        interact.element("close-details", |_style, b: &model::Button| b.click.fire(&()));
        assert_eq!(details_visible(), false);
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
        assert_eq!(details_text,
            "BuildID: 1234\n\
             ProductName: Bar\n\
             ReleaseChannel: release\n\
             SomeNestedJson: {\"foo\":\"bar\"}\n\
             SubmittedFrom: Client\n\
             TelemetryClientId: telemetry_client\n\
             TelemetryServerURL: https://telemetry.example.com\n\
             TelemetrySessionId: telemetry_session\n\
             Throttleable: 1\n\
             Vendor: FooCorp\n\
             Version: 100.0\n\
             This report also contains technical information about the state of the application when it crashed.\n"
        );
    });
}

#[test]
fn data_dir_default() {
    let mut test = GuiTest::new();
    test.config.data_dir = None;
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    test.assert_files()
        .set_data_dir("data_dir/FooCorp/Bar/Crash Reports")
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted()
        .pending();
}

#[test]
fn include_url() {
    for setting in [false, true] {
        let mut test = GuiTest::new();
        test.files.add_dir("data_dir").add_file(
            "data_dir/crashreporter_settings.json",
            Settings {
                submit_report: true,
                include_url: setting,
            }
            .to_string(),
        );
        test.mock
            .set(
                Command::mock("curl"),
                Box::new(|_| Err(std::io::ErrorKind::NotFound.into())),
            )
            .set(
                net::report::MockLibCurl,
                Box::new(move |report| {
                    assert_eq!(
                        report.extra.get("URL").and_then(|v| v.as_str()),
                        setting.then_some("https://url.example.com")
                    );
                    Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
                }),
            );
        test.run(|interact| {
            interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
        });
    }
}

#[test]
fn comment() {
    const COMMENT: &str = "My program crashed";

    for set_comment in [false, true] {
        let invoked = Counter::new();
        let mock_invoked = invoked.clone();
        let mut test = GuiTest::new();
        test.mock
            .set(
                Command::mock("curl"),
                Box::new(|_| Err(std::io::ErrorKind::NotFound.into())),
            )
            .set(
                net::report::MockLibCurl,
                Box::new(move |report| {
                    mock_invoked.inc();
                    assert_eq!(
                        report.extra.get("Comments").and_then(|v| v.as_str()),
                        set_comment.then_some(COMMENT)
                    );
                    Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
                }),
            );
        test.run(move |interact| {
            if set_comment {
                interact.element("comment", |_style, c: &model::TextBox| {
                    c.content.set(COMMENT.into())
                });
            }
            interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
        });

        invoked.assert_one();
    }
}

#[test]
fn curl_binary() {
    let mut test = GuiTest::new();
    test.files.add_file("minidump.memory.json.gz", "");
    let ran_process = Counter::new();
    let mock_ran_process = ran_process.clone();
    test.mock.set(
        Command::mock("curl"),
        Box::new(move |cmd| {
            if cmd.spawning {
                return Ok(crate::std::process::success_output());
            }

            // Curl strings need backslashes escaped.
            let curl_escaped_separator = if std::path::MAIN_SEPARATOR == '\\' {
                "\\\\"
            } else {
                std::path::MAIN_SEPARATOR_STR
            };

            let expected_args: Vec<OsString> = [
                "--user-agent",
                net::report::USER_AGENT,
                "--form",
                "extra=@-;filename=extra.json;type=application/json",
                "--form",
                &format!(
                    "upload_file_minidump=@\"data_dir{0}pending{0}minidump.dmp\"",
                    curl_escaped_separator
                ),
                "--form",
                &format!(
                    "memory_report=@\"data_dir{0}pending{0}minidump.memory.json.gz\"",
                    curl_escaped_separator
                ),
                "https://reports.example.com",
            ]
            .into_iter()
            .map(Into::into)
            .collect();
            assert_eq!(cmd.args, expected_args);
            let mut output = crate::std::process::success_output();
            output.stdout = format!("CrashID={MOCK_REMOTE_CRASH_ID}").into();
            mock_ran_process.inc();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    ran_process.assert_one();
}

#[test]
fn curl_library() {
    let invoked = Counter::new();
    let mock_invoked = invoked.clone();
    let mut test = GuiTest::new();
    test.mock
        .set(
            Command::mock("curl"),
            Box::new(|_| Err(std::io::ErrorKind::NotFound.into())),
        )
        .set(
            net::report::MockLibCurl,
            Box::new(move |_| {
                mock_invoked.inc();
                Ok(Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}")))
            }),
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });
    invoked.assert_one();
}

#[test]
fn report_not_sent() {
    let mut test = GuiTest::new();
    test.files.add_dir("events_dir");
    test.mock
        .set(
            Command::mock("curl"),
            Box::new(|_| Err(std::io::ErrorKind::NotFound.into())),
        )
        .set(
            net::report::MockLibCurl,
            Box::new(move |_| Err(std::io::ErrorKind::NotFound.into())),
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submission_event(false)
        .pending();
}

#[test]
fn report_response_failed() {
    let mut test = GuiTest::new();
    test.files.add_dir("events_dir");
    test.mock
        .set(
            Command::mock("curl"),
            Box::new(|_| Err(std::io::ErrorKind::NotFound.into())),
        )
        .set(
            net::report::MockLibCurl,
            Box::new(move |_| Ok(Err(std::io::ErrorKind::NotFound.into()))),
        );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submission_event(false)
        .pending();
}

#[test]
fn response_indicates_discarded() {
    let mut test = GuiTest::new();
    // A response indicating discarded triggers a prune of the directory containing the minidump.
    // Since there is one more minidump (the main one, minidump.dmp), pruning should keep all but
    // the first 3, which will be the oldest.
    const SHOULD_BE_PRUNED: usize = 3;

    for i in 0..MINIDUMP_PRUNE_SAVE_COUNT + SHOULD_BE_PRUNED - 1 {
        test.files.add_dir("data_dir/pending").add_file_result(
            format!("data_dir/pending/minidump{i}.dmp"),
            Ok("contents".into()),
            ::std::time::SystemTime::UNIX_EPOCH + ::std::time::Duration::from_secs(1234 + i as u64),
        );
        if i % 2 == 0 {
            test.files
                .add_file(format!("data_dir/pending/minidump{i}.extra"), "{}");
        }
        if i % 5 == 0 {
            test.files
                .add_file(format!("data_dir/pending/minidump{i}.memory.json.gz"), "{}");
        }
    }
    test.mock.set(
        Command::mock("curl"),
        Box::new(|_| {
            let mut output = crate::std::process::success_output();
            output.stdout = format!("CrashID={MOCK_REMOTE_CRASH_ID}\nDiscarded=1").into();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    let mut assert_files = test.assert_files();
    assert_files
        .ignore_log()
        .saved_settings(Settings::default())
        .pending();
    for i in SHOULD_BE_PRUNED..MINIDUMP_PRUNE_SAVE_COUNT + SHOULD_BE_PRUNED - 1 {
        assert_files.check_exists(format!("data_dir/pending/minidump{i}.dmp"));
        if i % 2 == 0 {
            assert_files.check_exists(format!("data_dir/pending/minidump{i}.extra"));
        }
        if i % 5 == 0 {
            assert_files.check_exists(format!("data_dir/pending/minidump{i}.memory.json.gz"));
        }
    }
}

#[test]
fn response_view_url() {
    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("curl"),
        Box::new(|_| {
            let mut output = crate::std::process::success_output();
            output.stdout =
                format!("CrashID={MOCK_REMOTE_CRASH_ID}\nViewURL=https://foo.bar.example").into();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .pending()
        .check(
            format!("data_dir/submitted/{MOCK_REMOTE_CRASH_ID}.txt"),
            format!(
                "\
                Crash ID: {}\n\
                You can view details of this crash at {}.\n",
                FluentArg(MOCK_REMOTE_CRASH_ID),
                FluentArg("https://foo.bar.example")
            ),
        );
}

#[test]
fn response_stop_sending_reports() {
    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("curl"),
        Box::new(|_| {
            let mut output = crate::std::process::success_output();
            output.stdout =
                format!("CrashID={MOCK_REMOTE_CRASH_ID}\nStopSendingReportsFor=100.0").into();
            Ok(output)
        }),
    );
    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted()
        .pending()
        .check_exists("data_dir/EndOfLife100.0");
}

/// A real temporary directory in the host filesystem.
///
/// The directory is guaranteed to be unique to the test suite process (in case of crash, it can be
/// inspected).
///
/// When dropped, the directory is deleted.
struct TempDir {
    path: ::std::path::PathBuf,
}

impl TempDir {
    /// Create a new directory with the given identifying name.
    ///
    /// The name should be unique to deconflict amongst concurrent tests.
    pub fn new(name: &str) -> Self {
        let path = ::std::env::temp_dir().join(format!(
            "{}-test-{}-{name}",
            env!("CARGO_PKG_NAME"),
            std::process::id()
        ));
        ::std::fs::create_dir_all(&path).unwrap();
        TempDir { path }
    }

    /// Get the temporary directory path.
    pub fn path(&self) -> &::std::path::Path {
        &self.path
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        // Best-effort removal, ignore errors.
        let _ = ::std::fs::remove_dir_all(&self.path);
    }
}

/// A mock crash report server.
///
/// When dropped, the server is shutdown.
struct TestCrashReportServer {
    addr: ::std::net::SocketAddr,
    shutdown_and_thread: Option<(
        tokio::sync::oneshot::Sender<()>,
        std::thread::JoinHandle<()>,
    )>,
}

impl TestCrashReportServer {
    /// Create and start a mock crash report server on an ephemeral port, returning a handle to the
    /// server.
    pub fn run() -> Self {
        let (shutdown, rx) = tokio::sync::oneshot::channel();

        use warp::Filter;

        let submit = warp::path("submit")
            .and(warp::filters::method::post())
            .and(warp::filters::header::header("content-type"))
            .and(warp::filters::body::bytes())
            .and_then(|content_type: String, body: bytes::Bytes| async move {
                let Some(boundary) = content_type.strip_prefix("multipart/form-data; boundary=")
                else {
                    return Err(warp::reject());
                };

                let body = String::from_utf8_lossy(&*body).to_owned();

                for part in body.split(&format!("--{boundary}")).skip(1) {
                    if part == "--\r\n" {
                        break;
                    }

                    let (_headers, _data) = part.split_once("\r\n\r\n").unwrap_or(("", part));
                    // TODO validate parts
                }
                Ok(format!("CrashID={MOCK_REMOTE_CRASH_ID}"))
            });

        let (addr_channel_tx, addr_channel_rx) = std::sync::mpsc::sync_channel(0);

        let thread = ::std::thread::spawn(move || {
            let rt = tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
                .expect("failed to create tokio runtime");
            let _guard = rt.enter();

            let (addr, server) =
                warp::serve(submit).bind_with_graceful_shutdown(([127, 0, 0, 1], 0), async move {
                    rx.await.ok();
                });

            addr_channel_tx.send(addr).unwrap();

            rt.block_on(server)
        });

        let addr = addr_channel_rx.recv().unwrap();

        TestCrashReportServer {
            addr,
            shutdown_and_thread: Some((shutdown, thread)),
        }
    }

    /// Get the url to which to submit crash reports for this mocked server.
    pub fn submit_url(&self) -> String {
        format!("http://{}/submit", self.addr)
    }
}

impl Drop for TestCrashReportServer {
    fn drop(&mut self) {
        let (shutdown, thread) = self.shutdown_and_thread.take().unwrap();
        let _ = shutdown.send(());
        thread.join().unwrap();
    }
}

#[test]
fn real_curl_binary() {
    if ::std::process::Command::new("curl").output().is_err() {
        eprintln!("no curl binary; skipping real_curl_binary test");
        return;
    }

    let server = TestCrashReportServer::run();

    let mut test = GuiTest::new();
    test.mock.set(
        Command::mock("curl"),
        Box::new(|cmd| cmd.output_from_real_command()),
    );
    test.config.report_url = Some(server.submit_url().into());
    test.config.delete_dump = true;

    // We need the dump file to actually exist since the curl binary is passed the file path.
    // The dump file needs to exist at the pending dir location.

    let tempdir = TempDir::new("real_curl_binary");
    let data_dir = tempdir.path().to_owned();
    let pending_dir = data_dir.join("pending");
    test.config.data_dir = Some(data_dir.clone().into());
    ::std::fs::create_dir_all(&pending_dir).unwrap();
    let dump_file = pending_dir.join("minidump.dmp");
    ::std::fs::write(&dump_file, MOCK_MINIDUMP_FILE).unwrap();

    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .set_data_dir(data_dir.display())
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted();
}

#[test]
fn real_curl_library() {
    if !crate::net::can_load_libcurl() {
        eprintln!("no libcurl; skipping real_libcurl test");
        return;
    }

    let server = TestCrashReportServer::run();

    let mut test = GuiTest::new();
    test.mock
        .set(
            Command::mock("curl"),
            Box::new(|_| Err(std::io::ErrorKind::NotFound.into())),
        )
        .set(mock::MockHook::new("use_system_libcurl"), true);
    test.config.report_url = Some(server.submit_url().into());
    test.config.delete_dump = true;

    // We need the dump file to actually exist since libcurl is passed the file path.
    // The dump file needs to exist at the pending dir location.

    let tempdir = TempDir::new("real_libcurl");
    let data_dir = tempdir.path().to_owned();
    let pending_dir = data_dir.join("pending");
    test.config.data_dir = Some(data_dir.clone().into());
    ::std::fs::create_dir_all(&pending_dir).unwrap();
    let dump_file = pending_dir.join("minidump.dmp");
    ::std::fs::write(&dump_file, MOCK_MINIDUMP_FILE).unwrap();

    test.run(|interact| {
        interact.element("quit", |_style, b: &model::Button| b.click.fire(&()));
    });

    test.assert_files()
        .set_data_dir(data_dir.display())
        .ignore_log()
        .saved_settings(Settings::default())
        .submitted();
}
