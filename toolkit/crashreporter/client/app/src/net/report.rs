/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Support for crash report creation and upload.
//!
//! Upload currently uses the system libcurl or curl binary rather than a rust network stack (as
//! curl is more mature, albeit the code to interact with it must be a bit more careful).

use crate::std::{ffi::OsStr, path::Path, process::Child};
use anyhow::Context;

#[cfg(mock)]
use crate::std::mock::{mock_key, MockKey};

#[cfg(mock)]
mock_key! {
    pub struct MockLibCurl => Box<dyn Fn(&CrashReport) -> std::io::Result<std::io::Result<String>> + Send + Sync>
}

pub const USER_AGENT: &str = concat!(env!("CARGO_PKG_NAME"), "/", env!("CARGO_PKG_VERSION"));

/// A crash report to upload.
///
/// Post a multipart form payload to the report URL.
///
/// The form data contains:
/// | name | filename | content | mime |
/// ====================================
/// | `extra` | `extra.json` | extra json object | `application/json`|
/// | `upload_file_minidump` | dump file name | dump file contents | derived (probably application/binary) |
/// if present:
/// | `memory_report` | memory file name | memory file contents | derived (probably gzipped json) |
pub struct CrashReport<'a> {
    pub extra: &'a serde_json::Value,
    pub dump_file: &'a Path,
    pub memory_file: Option<&'a Path>,
    pub url: &'a OsStr,
}

impl CrashReport<'_> {
    /// Send the crash report.
    pub fn send(&self) -> std::io::Result<CrashReportSender> {
        // Windows 10+ and macOS 10.15+ contain `curl` 7.64.1+ as a system-provided binary, so
        // `send_with_curl_binary` should not fail.
        //
        // Linux distros generally do not contain `curl`, but `libcurl` is very likely to be
        // incidentally installed (if not outright part of the distro base packages). Based on a
        // cursory look at the debian repositories as an examplar, the curl binary is much less
        // likely to be incidentally installed.
        //
        // For uniformity, we always will try the curl binary first, then try libcurl if that
        // fails.

        let extra_json_data = serde_json::to_string(self.extra)?;

        self.send_with_curl_binary(extra_json_data.clone())
            .or_else(|e| {
                log::info!("failed to invoke curl ({e}), trying libcurl");
                self.send_with_libcurl(extra_json_data.clone())
            })
    }

    /// Send the crash report using the `curl` binary.
    fn send_with_curl_binary(&self, extra_json_data: String) -> std::io::Result<CrashReportSender> {
        let mut cmd = crate::process::background_command("curl");

        cmd.args(["--user-agent", USER_AGENT]);

        cmd.arg("--form");
        // `@-` causes the data to be read from stdin, which is desirable to not have to worry
        // about process argument string length limitations (though they are generally pretty high
        // limits).
        cmd.arg("extra=@-;filename=extra.json;type=application/json");

        cmd.arg("--form");
        cmd.arg(format!(
            "upload_file_minidump=@{}",
            CurlQuote(&self.dump_file.display().to_string())
        ));

        if let Some(path) = self.memory_file {
            cmd.arg("--form");
            cmd.arg(format!(
                "memory_report=@{}",
                CurlQuote(&path.display().to_string())
            ));
        }

        cmd.arg(self.url);

        cmd.stdin(std::process::Stdio::piped());
        cmd.stdout(std::process::Stdio::piped());
        cmd.stderr(std::process::Stdio::piped());

        cmd.spawn().map(move |child| CrashReportSender::CurlChild {
            child,
            extra_json_data,
        })
    }

    /// Send the crash report using the `curl` library.
    fn send_with_libcurl(&self, extra_json_data: String) -> std::io::Result<CrashReportSender> {
        #[cfg(mock)]
        if !crate::std::mock::try_hook(false, "use_system_libcurl") {
            return self.send_with_mock_libcurl(extra_json_data);
        }

        let curl = super::libcurl::load()?;
        let mut easy = curl.easy()?;

        easy.set_url(&self.url.to_string_lossy())?;
        easy.set_user_agent(USER_AGENT)?;
        easy.set_max_redirs(30)?;

        let mut mime = easy.mime()?;
        {
            let mut part = mime.add_part()?;
            part.set_name("extra")?;
            part.set_filename("extra.json")?;
            part.set_type("application/json")?;
            part.set_data(extra_json_data.as_bytes())?;
        }
        {
            let mut part = mime.add_part()?;
            part.set_name("upload_file_minidump")?;
            part.set_filename(&self.dump_file.display().to_string())?;
            part.set_filedata(self.dump_file)?;
        }
        if let Some(path) = self.memory_file {
            let mut part = mime.add_part()?;
            part.set_name("memory_report")?;
            part.set_filename(&path.display().to_string())?;
            part.set_filedata(path)?;
        }
        easy.set_mime_post(mime)?;

        Ok(CrashReportSender::LibCurl { easy })
    }

    #[cfg(mock)]
    fn send_with_mock_libcurl(
        &self,
        _extra_json_data: String,
    ) -> std::io::Result<CrashReportSender> {
        MockLibCurl
            .get(|f| f(&self))
            .map(|response| CrashReportSender::MockLibCurl { response })
    }
}

pub enum CrashReportSender {
    CurlChild {
        child: Child,
        extra_json_data: String,
    },
    LibCurl {
        easy: super::libcurl::Easy<'static>,
    },
    #[cfg(mock)]
    MockLibCurl {
        response: std::io::Result<String>,
    },
}

impl CrashReportSender {
    pub fn finish(self) -> anyhow::Result<Response> {
        let response = match self {
            Self::CurlChild {
                mut child,
                extra_json_data,
            } => {
                {
                    let mut stdin = child
                        .stdin
                        .take()
                        .context("failed to get curl process stdin")?;
                    std::io::copy(&mut std::io::Cursor::new(extra_json_data), &mut stdin)
                        .context("failed to write extra file data to stdin of curl process")?;
                    // stdin is dropped at the end of this scope so that the stream gets an EOF,
                    // otherwise curl will wait for more input.
                }
                let output = child
                    .wait_with_output()
                    .context("failed to wait on curl process")?;
                anyhow::ensure!(
                    output.status.success(),
                    "process failed (exit status {}) with stderr: {}",
                    output.status,
                    String::from_utf8_lossy(&output.stderr)
                );
                String::from_utf8_lossy(&output.stdout).into_owned()
            }
            Self::LibCurl { easy } => {
                let response = easy.perform()?;
                let response_code = easy.get_response_code()?;

                let response = String::from_utf8_lossy(&response).into_owned();
                dbg!(&response, &response_code);

                anyhow::ensure!(
                    response_code == 200,
                    "unexpected response code ({response_code}): {response}"
                );

                response
            }
            #[cfg(mock)]
            Self::MockLibCurl { response } => response?.into(),
        };

        log::debug!("received response from sending report: {:?}", &*response);
        Ok(Response::parse(response))
    }
}

/// A parsed response from submitting a crash report.
#[derive(Default, Debug)]
pub struct Response {
    pub crash_id: Option<String>,
    pub stop_sending_reports_for: Option<String>,
    pub view_url: Option<String>,
    pub discarded: bool,
}

impl Response {
    /// Parse a server response.
    ///
    /// The response should be newline-separated `<key>=<value>` pairs.
    fn parse<S: AsRef<str>>(response: S) -> Self {
        let mut ret = Self::default();
        // Fields may be omitted, and parsing is best-effort but will not produce any errors (just
        // a default Response struct).
        for line in response.as_ref().lines() {
            if let Some((key, value)) = line.split_once('=') {
                match key {
                    "StopSendingReportsFor" => {
                        ret.stop_sending_reports_for = Some(value.to_owned())
                    }
                    "Discarded" => ret.discarded = true,
                    "CrashID" => ret.crash_id = Some(value.to_owned()),
                    "ViewURL" => ret.view_url = Some(value.to_owned()),
                    _ => (),
                }
            }
        }
        ret
    }
}

/// Quote a string per https://curl.se/docs/manpage.html#-F.
/// That is, add quote characters and escape " and \ with backslashes.
struct CurlQuote<'a>(&'a str);
impl std::fmt::Display for CurlQuote<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        use std::fmt::Write;

        f.write_char('"')?;
        const ESCAPE_CHARS: [char; 2] = ['"', '\\'];
        for substr in self.0.split_inclusive(ESCAPE_CHARS) {
            // The last string returned by `split_inclusive` may or may not contain the
            // search character, unfortunately.
            if substr.ends_with(ESCAPE_CHARS) {
                // Safe to use a byte offset rather than a character offset because the
                // ESCAPE_CHARS are each 1 byte in utf8.
                let (s, escape) = substr.split_at(substr.len() - 1);
                f.write_str(s)?;
                f.write_char('\\')?;
                f.write_str(escape)?;
            } else {
                f.write_str(substr)?;
            }
        }
        f.write_char('"')
    }
}
