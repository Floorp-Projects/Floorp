use crate::capabilities::{AndroidOptions};
use mozdevice::{Device, Host};
use mozprofile::profile::Profile;
use std::fmt;
use std::path::PathBuf;
use std::time;

// TODO: avoid port clashes across GeckoView-vehicles.
// For now, we always use target port 2829, leading to issues like bug 1533704.
const TARGET_PORT: u16 = 2829;

pub type Result<T> = std::result::Result<T, AndroidError>;

#[derive(Debug)]
pub enum AndroidError {
    ActivityNotFound(String),
    Device(mozdevice::DeviceError),
    NotConnected,
}

impl fmt::Display for AndroidError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            AndroidError::ActivityNotFound(ref package) => {
                write!(f, "Activity not found for package '{}'", package)
            },
            AndroidError::Device(ref message) => message.fmt(f),
            AndroidError::NotConnected =>
                write!(f, "Not connected to any Android device"),
        }

    }
}

impl From<mozdevice::DeviceError> for AndroidError {
    fn from(value: mozdevice::DeviceError) -> AndroidError {
        AndroidError::Device(value)
    }
}

/// A remote Gecko instance.
///
/// Host refers to the device running `geckodriver`.  Target refers to the
/// Android device running Gecko in a GeckoView-based vehicle.
#[derive(Debug)]
pub struct AndroidProcess {
    pub device: Device,
    pub package: String,
    pub activity: String,
}

impl AndroidProcess {
    pub fn new(
        device: Device,
        package: String,
        activity: String,
    ) -> mozdevice::Result<AndroidProcess> {
        Ok(AndroidProcess { device, package, activity })
    }
}

#[derive(Debug, Default)]
pub struct AndroidHandler {
    pub options: AndroidOptions,
    pub process: Option<AndroidProcess>,
    pub profile: PathBuf,

    // For port forwarding host => target
    pub host_port: u16,
    pub target_port: u16,
}

impl Drop for AndroidHandler {
    fn drop(&mut self) {
        // Try to clean up port forwarding.
        if let Some(ref process) = self.process {
            match process.device.kill_forward_port(self.host_port) {
                Ok(_) => debug!("Android port forward ({} -> {}) stopped",
                                &self.host_port, &self.target_port),
                Err(e) => error!("Android port forward ({} -> {}) failed to stop: {}",
                                 &self.host_port, &self.target_port, e),
            }
        }
    }
}

impl AndroidHandler {
    pub fn new(options: &AndroidOptions) -> AndroidHandler {
        // We need to push profile.pathbuf to a safe space on the device.
        // Make it per-Android package to avoid clashes and confusion.
        // This naming scheme follows GeckoView's configuration file naming scheme,
        // see bug 1533385.
        let profile = PathBuf::from(format!(
            "/mnt/sdcard/{}-geckodriver-profile", &options.package));

        AndroidHandler {
            options: options.clone(),
            profile,
            process: None,
            ..Default::default()
        }
    }

    pub fn connect(&mut self, host_port: u16) -> Result<()> {
        let host = Host {
            host: None,
            port: None,
            read_timeout: Some(time::Duration::from_millis(5000)),
            write_timeout: Some(time::Duration::from_millis(5000)),
        };

        let device = host.device_or_default(self.options.device_serial.as_ref())?;

        self.host_port = host_port;
        self.target_port = TARGET_PORT;

        // Set up port forward.  Port forwarding will be torn down, if possible,
        device.forward_port(self.host_port, self.target_port)?;
        debug!("Android port forward ({} -> {}) started", &self.host_port, &self.target_port);

        // If activity hasn't been specified default to the main activity of the package
        let activity = match self.options.activity {
            Some(ref activity) => activity.clone(),
            None => {
                let response = device.execute_host_shell_command(&format!(
                    "cmd package resolve-activity --brief {} | tail -n 1",
                    &self.options.package))?;
                let parts = response
                    .trim_end()
                    .split("/")
                    .collect::<Vec<&str>>();

                if parts.len() == 1 {
                    return Err(AndroidError::ActivityNotFound(self.options.package.clone()));
                }

                parts[1].to_owned()
            }
        };

        self.process = Some(AndroidProcess::new(
            device,
            self.options.package.clone(),
            activity,
        )?);

        Ok(())
    }

    pub fn prepare(&self, profile: &Profile) -> Result<()> {
        match self.process {
            Some(ref process) => {
                process.device.clear_app_data(&self.options.package)?;

                // These permissions, at least, are required to read profiles in /mnt/sdcard.
                for perm in &["READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE"] {
                    process.device.execute_host_shell_command(&format!(
                        "pm grant {} android.permission.{}", &self.options.package, perm))?;
                }

                debug!("Deleting {}", self.profile.display());
                process.device.execute_host_shell_command(&format!(
                    "rm -rf {}", self.profile.display()))?;

                debug!("Pushing {} to {}", profile.path.display(), self.profile.display());
                process.device.push_dir(&profile.path, &self.profile, 0o777)?;
            },
            None => return Err(AndroidError::NotConnected)
        }

        Ok(())
    }

    pub fn launch(&self) -> Result<()> {
        match self.process {
            Some(ref process) => {
                // TODO: Use GeckoView's local configuration file (bug 1577850) unless a special
                // "I'm Fennec" flag is set, indicating we should use command line arguments.
                // Fenix does not handle command line arguments at this time.
                let mut intent_arguments = self.options.intent_arguments.clone()
                    .unwrap_or_else(|| Vec::with_capacity(3));
                intent_arguments.push("--es".to_owned());
                intent_arguments.push("args".to_owned());
                intent_arguments.push(format!(
                    "-marionette -profile {}", self.profile.display()).to_owned());

                debug!("Launching {}/{}", process.package, process.activity);
                process.device
                    .launch(&process.package, &process.activity, &intent_arguments)
                    .map_err(|e| {
                        let message = format!(
                            "Could not launch Android {}/{}: {}", process.package, process.activity, e);
                        mozdevice::DeviceError::Adb(message)
                    })?;
            },
            None => return Err(AndroidError::NotConnected)
        }

        Ok(())
    }

    pub fn force_stop(&self) -> Result<()> {
        match &self.process {
            Some(process) => {
                debug!("Force stopping the Android package: {}", &process.package);
                process.device.force_stop(&process.package)?;
            },
            None => return Err(AndroidError::NotConnected)
        }

        Ok(())
   }
}
