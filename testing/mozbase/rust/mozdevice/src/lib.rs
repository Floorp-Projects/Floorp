/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate log;
extern crate once_cell;
extern crate regex;
extern crate tempfile;
extern crate walkdir;

pub mod adb;
pub mod shell;

#[cfg(test)]
pub mod test;

use once_cell::sync::Lazy;
use regex::Regex;
use std::collections::BTreeMap;
use std::convert::TryFrom;
use std::fmt;
use std::fs::File;
use std::io::{self, Read, Write};
use std::iter::FromIterator;
use std::net::TcpStream;
use std::num::{ParseIntError, TryFromIntError};
use std::path::{Component, Path};
use std::str::{FromStr, Utf8Error};
use std::time::{Duration, SystemTime};
pub use unix_path::{Path as UnixPath, PathBuf as UnixPathBuf};
use uuid::Uuid;
use walkdir::WalkDir;

use crate::adb::{DeviceSerial, SyncCommand};

pub type Result<T> = std::result::Result<T, DeviceError>;

static SYNC_REGEX: Lazy<Regex> = Lazy::new(|| Regex::new(r"[^A-Za-z0-9_@%+=:,./-]").unwrap());

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum AndroidStorageInput {
    Auto,
    App,
    Internal,
    Sdcard,
}

impl FromStr for AndroidStorageInput {
    type Err = DeviceError;

    fn from_str(s: &str) -> Result<Self> {
        match s {
            "auto" => Ok(AndroidStorageInput::Auto),
            "app" => Ok(AndroidStorageInput::App),
            "internal" => Ok(AndroidStorageInput::Internal),
            "sdcard" => Ok(AndroidStorageInput::Sdcard),
            _ => Err(DeviceError::InvalidStorage),
        }
    }
}

impl Default for AndroidStorageInput {
    fn default() -> Self {
        AndroidStorageInput::Auto
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum AndroidStorage {
    App,
    Internal,
    Sdcard,
}

#[derive(Debug)]
pub enum DeviceError {
    Adb(String),
    FromInt(TryFromIntError),
    InvalidStorage,
    Io(io::Error),
    MissingPackage,
    MultipleDevices,
    ParseInt(ParseIntError),
    UnknownDevice(String),
    Utf8(Utf8Error),
    WalkDir(walkdir::Error),
}

impl fmt::Display for DeviceError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            DeviceError::Adb(ref message) => message.fmt(f),
            DeviceError::FromInt(ref int) => int.fmt(f),
            DeviceError::InvalidStorage => write!(f, "Invalid storage"),
            DeviceError::Io(ref error) => error.fmt(f),
            DeviceError::MissingPackage => write!(f, "Missing package"),
            DeviceError::MultipleDevices => write!(f, "Multiple Android devices online"),
            DeviceError::ParseInt(ref error) => error.fmt(f),
            DeviceError::UnknownDevice(ref serial) => {
                write!(f, "Unknown Android device with serial '{}'", serial)
            }
            DeviceError::Utf8(ref error) => error.fmt(f),
            DeviceError::WalkDir(ref error) => error.fmt(f),
        }
    }
}

impl From<io::Error> for DeviceError {
    fn from(value: io::Error) -> DeviceError {
        DeviceError::Io(value)
    }
}

impl From<ParseIntError> for DeviceError {
    fn from(value: ParseIntError) -> DeviceError {
        DeviceError::ParseInt(value)
    }
}

impl From<TryFromIntError> for DeviceError {
    fn from(value: TryFromIntError) -> DeviceError {
        DeviceError::FromInt(value)
    }
}

impl From<Utf8Error> for DeviceError {
    fn from(value: Utf8Error) -> DeviceError {
        DeviceError::Utf8(value)
    }
}

impl From<walkdir::Error> for DeviceError {
    fn from(value: walkdir::Error) -> DeviceError {
        DeviceError::WalkDir(value)
    }
}

fn encode_message(payload: &str) -> Result<String> {
    let hex_length = u16::try_from(payload.len()).map(|len| format!("{:0>4X}", len))?;

    Ok(format!("{}{}", hex_length, payload))
}

fn parse_device_info(line: &str) -> Option<DeviceInfo> {
    // Turn "serial\tdevice key1:value1 key2:value2 ..." into a `DeviceInfo`.
    let mut pairs = line.split_whitespace();
    let serial = pairs.next();
    let state = pairs.next();
    if let (Some(serial), Some("device")) = (serial, state) {
        let info: BTreeMap<String, String> = pairs
            .filter_map(|pair| {
                let mut kv = pair.split(':');
                if let (Some(k), Some(v), None) = (kv.next(), kv.next(), kv.next()) {
                    Some((k.to_owned(), v.to_owned()))
                } else {
                    None
                }
            })
            .collect();

        Some(DeviceInfo {
            serial: serial.to_owned(),
            info,
        })
    } else {
        None
    }
}

/// Reads the payload length of a host message from the stream.
fn read_length<R: Read>(stream: &mut R) -> Result<usize> {
    let mut bytes: [u8; 4] = [0; 4];
    stream.read_exact(&mut bytes)?;

    let response = std::str::from_utf8(&bytes)?;

    Ok(usize::from_str_radix(&response, 16)?)
}

/// Reads the payload length of a device message from the stream.
fn read_length_little_endian(reader: &mut dyn Read) -> Result<usize> {
    let mut bytes: [u8; 4] = [0; 4];
    reader.read_exact(&mut bytes)?;

    let n: usize = (bytes[0] as usize)
        + ((bytes[1] as usize) << 8)
        + ((bytes[2] as usize) << 16)
        + ((bytes[3] as usize) << 24);

    Ok(n)
}

/// Writes the payload length of a device message to the stream.
fn write_length_little_endian(writer: &mut dyn Write, n: usize) -> Result<usize> {
    let mut bytes = [0; 4];
    bytes[0] = (n & 0xFF) as u8;
    bytes[1] = ((n >> 8) & 0xFF) as u8;
    bytes[2] = ((n >> 16) & 0xFF) as u8;
    bytes[3] = ((n >> 24) & 0xFF) as u8;

    writer.write(&bytes[..]).map_err(DeviceError::Io)
}

fn read_response(stream: &mut TcpStream, has_output: bool, has_length: bool) -> Result<Vec<u8>> {
    let mut bytes: [u8; 1024] = [0; 1024];

    stream.read_exact(&mut bytes[0..4])?;

    if &bytes[0..4] != SyncCommand::Okay.code() {
        let n = bytes.len().min(read_length(stream)?);
        stream.read_exact(&mut bytes[0..n])?;

        let message = std::str::from_utf8(&bytes[0..n]).map(|s| format!("adb error: {}", s))?;

        return Err(DeviceError::Adb(message));
    }

    let mut response = Vec::new();

    if has_output {
        stream.read_to_end(&mut response)?;

        if response.len() >= 4 && &response[0..4] == SyncCommand::Okay.code() {
            // Sometimes the server produces OKAYOKAY.  Sometimes there is a transport OKAY and
            // then the underlying command OKAY.  This is straight from `chromedriver`.
            response = response.split_off(4);
        }

        if response.len() >= 4 && &response[0..4] == SyncCommand::Fail.code() {
            // The server may even produce OKAYFAIL, which means the underlying
            // command failed. First split-off the `FAIL` and length of the message.
            response = response.split_off(8);

            let message = std::str::from_utf8(&*response).map(|s| format!("adb error: {}", s))?;

            return Err(DeviceError::Adb(message));
        }

        if has_length {
            if response.len() >= 4 {
                let message = response.split_off(4);
                let slice: &mut &[u8] = &mut &*response;

                let n = read_length(slice)?;
                warn!(
                    "adb server response contained hexstring length {} and message length was {} \
                     and message was {:?}",
                    n,
                    message.len(),
                    std::str::from_utf8(&message)?
                );

                return Ok(message);
            } else {
                return Err(DeviceError::Adb(format!(
                    "adb server response did not contain expected hexstring length: {:?}",
                    std::str::from_utf8(&response)?
                )));
            }
        }
    }

    Ok(response)
}

/// Detailed information about an ADB device.
#[derive(Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct DeviceInfo {
    pub serial: DeviceSerial,
    pub info: BTreeMap<String, String>,
}

/// Represents a connection to an ADB host, which multiplexes the connections to
/// individual devices.
#[derive(Debug)]
pub struct Host {
    /// The TCP host to connect to.  Defaults to `"localhost"`.
    pub host: Option<String>,
    /// The TCP port to connect to.  Defaults to `5037`.
    pub port: Option<u16>,
    /// Optional TCP read timeout duration.  Defaults to 2s.
    pub read_timeout: Option<Duration>,
    /// Optional TCP write timeout duration.  Defaults to 2s.
    pub write_timeout: Option<Duration>,
}

impl Default for Host {
    fn default() -> Host {
        Host {
            host: Some("localhost".to_string()),
            port: Some(5037),
            read_timeout: Some(Duration::from_secs(2)),
            write_timeout: Some(Duration::from_secs(2)),
        }
    }
}

impl Host {
    /// Searches for available devices, and selects the one as specified by `device_serial`.
    ///
    /// If multiple devices are online, and no device has been specified,
    /// the `ANDROID_SERIAL` environment variable can be used to select one.
    pub fn device_or_default<T: AsRef<str>>(
        self,
        device_serial: Option<&T>,
        storage: AndroidStorageInput,
    ) -> Result<Device> {
        let serials: Vec<String> = self
            .devices::<Vec<_>>()?
            .into_iter()
            .map(|d| d.serial)
            .collect();

        if let Some(ref serial) = device_serial
            .map(|v| v.as_ref().to_owned())
            .or_else(|| std::env::var("ANDROID_SERIAL").ok())
        {
            if !serials.contains(serial) {
                return Err(DeviceError::UnknownDevice(serial.clone()));
            }

            return Device::new(self, serial.to_owned(), storage);
        }

        if serials.len() > 1 {
            return Err(DeviceError::MultipleDevices);
        }

        if let Some(ref serial) = serials.first() {
            return Device::new(self, serial.to_owned().to_string(), storage);
        }

        Err(DeviceError::Adb("No Android devices are online".to_owned()))
    }

    pub fn connect(&self) -> Result<TcpStream> {
        let stream = TcpStream::connect(format!(
            "{}:{}",
            self.host.clone().unwrap_or_else(|| "localhost".to_owned()),
            self.port.unwrap_or(5037)
        ))?;
        stream.set_read_timeout(self.read_timeout)?;
        stream.set_write_timeout(self.write_timeout)?;
        Ok(stream)
    }

    pub fn execute_command(
        &self,
        command: &str,
        has_output: bool,
        has_length: bool,
    ) -> Result<String> {
        let mut stream = self.connect()?;

        stream.write_all(encode_message(command)?.as_bytes())?;
        let bytes = read_response(&mut stream, has_output, has_length)?;
        // TODO: should we assert no bytes were read?

        let response = std::str::from_utf8(&bytes)?;

        Ok(response.to_owned())
    }

    pub fn execute_host_command(
        &self,
        host_command: &str,
        has_length: bool,
        has_output: bool,
    ) -> Result<String> {
        self.execute_command(&format!("host:{}", host_command), has_output, has_length)
    }

    pub fn features<B: FromIterator<String>>(&self) -> Result<B> {
        let features = self.execute_host_command("features", true, true)?;
        Ok(features.split(',').map(|x| x.to_owned()).collect())
    }

    pub fn devices<B: FromIterator<DeviceInfo>>(&self) -> Result<B> {
        let response = self.execute_host_command("devices-l", true, true)?;

        let infos: B = response.lines().filter_map(parse_device_info).collect();

        Ok(infos)
    }
}

/// Represents an ADB device.
#[derive(Debug)]
pub struct Device {
    /// ADB host that controls this device.
    pub host: Host,

    /// Serial number uniquely identifying this ADB device.
    pub serial: DeviceSerial,

    /// adb running as root
    pub adbd_root: bool,

    /// Flag for rooted device
    pub is_rooted: bool,

    /// "su 0" command available
    pub su_0_root: bool,

    /// "su -c" command available
    pub su_c_root: bool,

    pub run_as_package: Option<String>,

    pub storage: AndroidStorage,

    /// Cache intermediate tempfile name used in pushing via run_as.
    pub tempfile: UnixPathBuf,
}

impl Device {
    pub fn new(host: Host, serial: DeviceSerial, storage: AndroidStorageInput) -> Result<Device> {
        let mut device = Device {
            host,
            serial,
            adbd_root: false,
            is_rooted: false,
            run_as_package: None,
            storage: AndroidStorage::App,
            su_c_root: false,
            su_0_root: false,
            tempfile: UnixPathBuf::from("/data/local/tmp"),
        };
        device
            .tempfile
            .push(Uuid::new_v4().to_hyphenated().to_string());

        // check for rooted devices
        let uid_check = |id: String| id.contains("uid=0");
        device.adbd_root = device
            .execute_host_shell_command("id")
            .map_or(false, uid_check);
        device.su_0_root = device
            .execute_host_shell_command("su 0 id")
            .map_or(false, uid_check);
        device.su_c_root = device
            .execute_host_shell_command("su -c id")
            .map_or(false, uid_check);
        device.is_rooted = device.adbd_root || device.su_0_root || device.su_c_root;

        device.storage = match storage {
            AndroidStorageInput::App => AndroidStorage::App,
            AndroidStorageInput::Internal => AndroidStorage::Internal,
            AndroidStorageInput::Sdcard => AndroidStorage::Sdcard,
            AndroidStorageInput::Auto => AndroidStorage::Sdcard,
        };

        if device.is_rooted {
            debug!("Device is rooted");

            // Set Permissive=1 if we have root.
            device.execute_host_shell_command("setenforce permissive")?;
        } else {
            debug!("Device is unrooted");
        }

        Ok(device)
    }

    pub fn clear_app_data(&self, package: &str) -> Result<bool> {
        self.execute_host_shell_command(&format!("pm clear {}", package))
            .map(|v| v.contains("Success"))
    }

    pub fn create_dir(&self, path: &UnixPath) -> Result<()> {
        debug!("Creating {}", path.display());

        let enable_run_as = self.enable_run_as_for_path(&path);
        self.execute_host_shell_command_as(&format!("mkdir -p {}", path.display()), enable_run_as)?;

        Ok(())
    }

    pub fn chmod(&self, path: &UnixPath, mask: &str, recursive: bool) -> Result<()> {
        let enable_run_as = self.enable_run_as_for_path(&path);

        let recursive = match recursive {
            true => " -R",
            false => "",
        };

        self.execute_host_shell_command_as(
            &format!("chmod {} {} {}", recursive, mask, path.display()),
            enable_run_as,
        )?;

        Ok(())
    }

    pub fn execute_host_command(
        &self,
        command: &str,
        has_output: bool,
        has_length: bool,
    ) -> Result<String> {
        let mut stream = self.host.connect()?;

        let switch_command = format!("host:transport:{}", self.serial);
        debug!("execute_host_command: >> {:?}", &switch_command);
        stream.write_all(encode_message(&switch_command)?.as_bytes())?;
        let _bytes = read_response(&mut stream, false, false)?;
        debug!("execute_host_command: << {:?}", _bytes);
        // TODO: should we assert no bytes were read?

        debug!("execute_host_command: >> {:?}", &command);
        stream.write_all(encode_message(&command)?.as_bytes())?;
        let bytes = read_response(&mut stream, has_output, has_length)?;

        let response = std::str::from_utf8(&bytes)?;
        debug!("execute_host_command: << {:?}", response);

        // Unify new lines by removing possible carriage returns
        Ok(response.replace("\r\n", "\n"))
    }

    pub fn enable_run_as_for_path(&self, path: &UnixPath) -> bool {
        match &self.run_as_package {
            Some(package) => {
                let mut p = UnixPathBuf::from("/data/data/");
                p.push(package);
                path.starts_with(p)
            }
            None => false,
        }
    }

    pub fn execute_host_shell_command(&self, shell_command: &str) -> Result<String> {
        self.execute_host_shell_command_as(shell_command, false)
    }

    pub fn execute_host_shell_command_as(
        &self,
        shell_command: &str,
        enable_run_as: bool,
    ) -> Result<String> {
        // We don't want to duplicate su invocations.
        if shell_command.starts_with("su") {
            return self.execute_host_command(&format!("shell:{}", shell_command), true, false);
        }

        let has_outer_quotes = shell_command.starts_with('"') && shell_command.ends_with('"')
            || shell_command.starts_with('\'') && shell_command.ends_with('\'');

        if self.adbd_root {
            return self.execute_host_command(&format!("shell:{}", shell_command), true, false);
        }

        if self.su_0_root {
            return self.execute_host_command(
                &format!("shell:su 0 {}", shell_command),
                true,
                false,
            );
        }

        if self.su_c_root {
            if has_outer_quotes {
                return self.execute_host_command(
                    &format!("shell:su -c {}", shell_command),
                    true,
                    false,
                );
            }

            if SYNC_REGEX.is_match(shell_command) {
                let arg: &str = &shell_command.replace("'", "'\"'\"'")[..];
                return self.execute_host_command(&format!("shell:su -c '{}'", arg), true, false);
            }

            return self.execute_host_command(
                &format!("shell:su -c \"{}\"", shell_command),
                true,
                false,
            );
        }

        // Execute command as package
        if enable_run_as {
            let run_as_package = self
                .run_as_package
                .as_ref()
                .ok_or(DeviceError::MissingPackage)?;

            if has_outer_quotes {
                return self.execute_host_command(
                    &format!("shell:run-as {} {}", run_as_package, shell_command),
                    true,
                    false,
                );
            }

            if SYNC_REGEX.is_match(shell_command) {
                let arg: &str = &shell_command.replace("'", "'\"'\"'")[..];
                return self.execute_host_command(
                    &format!("shell:run-as {} {}", run_as_package, arg),
                    true,
                    false,
                );
            }

            return self.execute_host_command(
                &format!("shell:run-as {} \"{}\"", run_as_package, shell_command),
                true,
                false,
            );
        }

        self.execute_host_command(&format!("shell:{}", shell_command), true, false)
    }

    pub fn is_app_installed(&self, package: &str) -> Result<bool> {
        self.execute_host_shell_command(&format!("pm path {}", package))
            .map(|v| v.contains("package:"))
    }

    pub fn launch<T: AsRef<str>>(
        &self,
        package: &str,
        activity: &str,
        am_start_args: &[T],
    ) -> Result<bool> {
        let mut am_start = format!("am start -W -n {}/{}", package, activity);

        for arg in am_start_args {
            am_start.push(' ');
            if SYNC_REGEX.is_match(arg.as_ref()) {
                am_start.push_str(&format!("\"{}\"", &shell::escape(arg.as_ref())));
            } else {
                am_start.push_str(&shell::escape(arg.as_ref()));
            };
        }

        self.execute_host_shell_command(&am_start)
            .map(|v| v.contains("Complete"))
    }

    pub fn force_stop(&self, package: &str) -> Result<()> {
        debug!("Force stopping Android package: {}", package);
        self.execute_host_shell_command(&format!("am force-stop {}", package))
            .and(Ok(()))
    }

    pub fn forward_port(&self, local: u16, remote: u16) -> Result<u16> {
        let command = format!(
            "host-serial:{}:forward:tcp:{};tcp:{}",
            self.serial, local, remote
        );
        let response = self.host.execute_command(&command, true, false)?;

        if local == 0 {
            Ok(response.parse::<u16>()?)
        } else {
            Ok(local)
        }
    }

    pub fn kill_forward_port(&self, local: u16) -> Result<()> {
        let command = format!("killforward:tcp:{}", local);
        self.execute_host_command(&command, true, false).and(Ok(()))
    }

    pub fn kill_forward_all_ports(&self) -> Result<()> {
        self.execute_host_command(&"killforward-all".to_owned(), false, false)
            .and(Ok(()))
    }

    pub fn reverse_port(&self, remote: u16, local: u16) -> Result<u16> {
        let command = format!("reverse:forward:tcp:{};tcp:{}", remote, local);
        let response = self.execute_host_command(&command, true, false)?;

        if remote == 0 {
            Ok(response.parse::<u16>()?)
        } else {
            Ok(remote)
        }
    }

    pub fn kill_reverse_port(&self, remote: u16) -> Result<()> {
        let command = format!("reverse:killforward:tcp:{}", remote);
        self.execute_host_command(&command, true, true).and(Ok(()))
    }

    pub fn kill_reverse_all_ports(&self) -> Result<()> {
        let command = "reverse:killforward-all".to_owned();
        self.execute_host_command(&command, false, false)
            .and(Ok(()))
    }

    pub fn path_exists(&self, path: &UnixPath, enable_run_as: bool) -> Result<bool> {
        self.execute_host_shell_command_as(format!("ls {}", path.display()).as_str(), enable_run_as)
            .map(|path| !path.contains("No such file or directory"))
    }

    pub fn push(&self, buffer: &mut dyn Read, dest: &UnixPath, mode: u32) -> Result<()> {
        // Implement the ADB protocol to send a file to the device.
        // The protocol consists of the following steps:
        // * Send "host:transport" command with device serial
        // * Send "sync:" command to initialize file transfer
        // * Send "SEND" command with name and mode of the file
        // * Send "DATA" command one or more times for the file content
        // * Send "DONE" command to indicate end of file transfer

        let enable_run_as = self.enable_run_as_for_path(&dest.to_path_buf());
        let dest1 = match enable_run_as {
            true => self.tempfile.as_path(),
            false => UnixPath::new(dest),
        };

        // If the destination directory does not exist, adb will
        // create it and any necessary ancestors however it will not
        // set the directory permissions to 0o777.  In addition,
        // Android 9 (P) has a bug in its push implementation which
        // will cause a push which creates directories to fail with
        // the error `secure_mkdirs failed: Operation not
        // permitted`. We can work around this by creating the
        // destination directories prior to the push.  Collect the
        // ancestors of the destination directory which do not yet
        // exist so we can create them and adjust their permissions
        // prior to performing the push.
        let mut current = dest.parent();
        let mut leaf: Option<&UnixPath> = None;
        let mut root: Option<&UnixPath> = None;

        while let Some(path) = current {
            if self.path_exists(path, enable_run_as)? {
                break;
            }
            if leaf.is_none() {
                leaf = Some(path);
            }
            root = Some(path);
            current = path.parent();
        }

        if let Some(path) = leaf {
            self.create_dir(&path)?;
        }

        if let Some(path) = root {
            self.chmod(&path, "777", true)?;
        }

        let mut stream = self.host.connect()?;

        let message = encode_message(&format!("host:transport:{}", self.serial))?;
        stream.write_all(message.as_bytes())?;
        let _bytes = read_response(&mut stream, false, true)?;

        let message = encode_message("sync:")?;
        stream.write_all(message.as_bytes())?;
        let _bytes = read_response(&mut stream, false, true)?;

        stream.write_all(SyncCommand::Send.code())?;
        let args_ = format!("{},{}", dest1.display(), mode);
        let args = args_.as_bytes();
        write_length_little_endian(&mut stream, args.len())?;
        stream.write_all(args)?;

        // Use a 32KB buffer to transfer the file contents
        // TODO: Maybe adjust to maxdata (256KB)
        let mut buf = [0; 32 * 1024];

        loop {
            let len = buffer.read(&mut buf)?;

            if len == 0 {
                break;
            }

            stream.write_all(SyncCommand::Data.code())?;
            write_length_little_endian(&mut stream, len)?;
            stream.write_all(&buf[0..len])?;
        }

        // https://android.googlesource.com/platform/system/core/+/master/adb/SYNC.TXT#66
        //
        // When the file is transferred a sync request "DONE" is sent, where length is set
        // to the last modified time for the file. The server responds to this last
        // request (but not to chunk requests) with an "OKAY" sync response (length can
        // be ignored).
        let time: u32 = ((SystemTime::now().duration_since(SystemTime::UNIX_EPOCH))
            .unwrap()
            .as_secs()
            & 0xFFFF_FFFF) as u32;

        stream.write_all(SyncCommand::Done.code())?;
        write_length_little_endian(&mut stream, time as usize)?;

        // Status.
        stream.read_exact(&mut buf[0..4])?;

        if &buf[0..4] == SyncCommand::Okay.code() {
            if enable_run_as {
                // Use cp -a to preserve the permissions set by push.
                let result = self.execute_host_shell_command_as(
                    format!("cp -aR {} {}", dest1.display(), dest.display()).as_str(),
                    enable_run_as,
                );
                if self.remove(dest1).is_err() {
                    debug!("Failed to remove {}", dest1.display());
                }
                result?;
            }
            Ok(())
        } else if &buf[0..4] == SyncCommand::Fail.code() {
            if enable_run_as && self.remove(dest1).is_err() {
                debug!("Failed to remove {}", dest1.display());
            }
            let n = buf.len().min(read_length_little_endian(&mut stream)?);

            stream.read_exact(&mut buf[0..n])?;

            let message = std::str::from_utf8(&buf[0..n])
                .map(|s| format!("adb error: {}", s))
                .unwrap_or_else(|_| "adb error was not utf-8".into());

            Err(DeviceError::Adb(message))
        } else {
            if self.remove(dest1).is_err() {
                debug!("Failed to remove {}", dest1.display());
            }
            Err(DeviceError::Adb("FAIL (unknown)".to_owned()))
        }
    }

    pub fn push_dir(&self, source: &Path, dest_dir: &UnixPath, mode: u32) -> Result<()> {
        debug!("Pushing {} to {}", source.display(), dest_dir.display());

        let walker = WalkDir::new(source).follow_links(false).into_iter();

        for entry in walker {
            let entry = entry?;
            let path = entry.path();

            if !entry.metadata()?.is_file() {
                continue;
            }

            let mut file = File::open(path)?;

            let tail = path
                .strip_prefix(source)
                .map_err(|e| io::Error::new(io::ErrorKind::Other, e.to_string()))?;

            let dest = append_components(dest_dir, tail)?;
            self.push(&mut file, &dest, mode)?;
        }

        Ok(())
    }

    pub fn remove(&self, path: &UnixPath) -> Result<()> {
        debug!("Deleting {}", path.display());

        self.execute_host_shell_command_as(
            &format!("rm -rf {}", path.display()),
            self.enable_run_as_for_path(&path),
        )?;

        Ok(())
    }
}

pub(crate) fn append_components(
    base: &UnixPath,
    tail: &Path,
) -> std::result::Result<UnixPathBuf, io::Error> {
    let mut buf = base.to_path_buf();

    for component in tail.components() {
        if let Component::Normal(ref segment) = component {
            let utf8 = segment.to_str().ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::Other,
                    "Could not represent path segment as UTF-8",
                )
            })?;
            buf.push(utf8);
        } else {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                "Unexpected path component".to_owned(),
            ));
        }
    }

    Ok(buf)
}
