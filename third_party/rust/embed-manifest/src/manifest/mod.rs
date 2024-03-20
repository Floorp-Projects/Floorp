//! A builder for Windows application manifest XML files.
//!
//! This module allows the construction of application manifests from code with the
//! [`ManifestBuilder`], for use from a Cargo build script. Once configured, the builder
//! should be passed to [`embed_manifest()`][crate::embed_manifest] to generate the
//! correct instructions for Cargo. For any other use, the builder will output the XML
//! when formatted for [`Display`], or with [`to_string()`][ToString]. For more
//! information about the different elements of an application manifest, see
//! [Application Manifests][1] in the Microsoft Windows App Development documentation.
//!
//! [1]: https://docs.microsoft.com/en-us/windows/win32/sbscs/application-manifests
//!
//! To generate the manifest XML separately, the XML can be output with `write!` or
//! copied to a string with [`to_string()`][ToString]. To produce the manifest XML with
//! extra whitespace for formatting, format it with the ‘alternate’ flag:
//!
//! ```
//! # use embed_manifest::new_manifest;
//! let builder = new_manifest("Company.OrgUnit.Program");
//! assert_eq!(format!("{:#}", builder), r#"<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
//! <assembly xmlns="urn:schemas-microsoft-com:asm.v1" xmlns:asmv3="urn:schemas-microsoft-com:asm.v3" manifestVersion="1.0">
//!     <assemblyIdentity name="Company.OrgUnit.Program" type="win32" version="1.4.0.0"/>
//!     <dependency>
//!         <dependentAssembly>
//!             <assemblyIdentity language="*" name="Microsoft.Windows.Common-Controls" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" type="win32" version="6.0.0.0"/>
//!         </dependentAssembly>
//!     </dependency>
//!     <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
//!         <application>
//!             <maxversiontested Id="10.0.18362.1"/>
//!             <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}"/>
//!             <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}"/>
//!             <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>
//!             <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
//!         </application>
//!     </compatibility>
//!     <asmv3:application>
//!         <asmv3:windowsSettings>
//!             <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
//!             <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">permonitorv2</dpiAwareness>
//!             <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
//!             <printerDriverIsolation xmlns="http://schemas.microsoft.com/SMI/2011/WindowsSettings">true</printerDriverIsolation>
//!         </asmv3:windowsSettings>
//!     </asmv3:application>
//!     <asmv3:trustInfo>
//!         <asmv3:security>
//!             <asmv3:requestedPrivileges>
//!                 <asmv3:requestedExecutionLevel level="asInvoker" uiAccess="false"/>
//!             </asmv3:requestedPrivileges>
//!         </asmv3:security>
//!     </asmv3:trustInfo>
//! </assembly>"#.replace("\n", "\r\n"))
//! ```

use std::fmt::{Display, Formatter};
use std::ops::RangeBounds;
use std::{env, fmt};

use crate::manifest::xml::XmlFormatter;

mod xml;

#[cfg(test)]
mod test;

/// An opaque container to describe the Windows application manifest for the
/// executable. A new instance with reasonable defaults is created with
/// [`new_manifest()`][crate::new_manifest].
#[derive(Debug)]
pub struct ManifestBuilder {
    identity: Option<AssemblyIdentity>,
    dependent_assemblies: Vec<AssemblyIdentity>,
    compatibility: ApplicationCompatibility,
    windows_settings: WindowsSettings,
    requested_execution_level: Option<RequestedExecutionLevel>,
}

impl ManifestBuilder {
    pub(crate) fn new(name: &str) -> Self {
        ManifestBuilder {
            identity: Some(AssemblyIdentity::application(name)),
            dependent_assemblies: vec![AssemblyIdentity::new(
                "Microsoft.Windows.Common-Controls",
                [6, 0, 0, 0],
                0x6595b64144ccf1df,
            )],
            compatibility: ApplicationCompatibility {
                max_version_tested: Some(MaxVersionTested::Windows10Version1903),
                supported_os: vec![
                    SupportedOS::Windows7,
                    SupportedOS::Windows8,
                    SupportedOS::Windows81,
                    SupportedOS::Windows10,
                ],
            },
            windows_settings: WindowsSettings::new(),
            requested_execution_level: Some(RequestedExecutionLevel {
                level: ExecutionLevel::AsInvoker,
                ui_access: false,
            }),
        }
    }

    pub(crate) fn empty() -> Self {
        ManifestBuilder {
            identity: None,
            dependent_assemblies: Vec::new(),
            compatibility: ApplicationCompatibility {
                max_version_tested: None,
                supported_os: Vec::new(),
            },
            windows_settings: WindowsSettings::empty(),
            requested_execution_level: None,
        }
    }

    // Set the dot-separated [application name][identity] in the manifest.
    //
    // [identity]: https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests#assemblyIdentity
    pub fn name(mut self, name: &str) -> Self {
        match self.identity {
            Some(ref mut identity) => identity.name = name.to_string(),
            None => self.identity = Some(AssemblyIdentity::application_version(name, 0, 0, 0, 0)),
        }
        self
    }

    /// Set the four-part application version number in the manifest.
    pub fn version(mut self, major: u16, minor: u16, build: u16, revision: u16) -> Self {
        match self.identity {
            Some(ref mut identity) => identity.version = Version(major, minor, build, revision),
            None => {
                self.identity = Some(AssemblyIdentity::application_version("", major, minor, build, revision));
            }
        }
        self
    }

    /// Add a dependency on a specific version of a side-by-side assembly
    /// to the application manifest. For more information on side-by-side
    /// assemblies, see [Using Side-by-side Assemblies][sxs].
    ///
    /// [sxs]: https://docs.microsoft.com/en-us/windows/win32/sbscs/using-side-by-side-assemblies
    pub fn dependency(mut self, identity: AssemblyIdentity) -> Self {
        self.dependent_assemblies.push(identity);
        self
    }

    /// Remove a dependency on a side-by-side assembly. This can be used to
    /// remove the default dependency on Common Controls version 6:
    ///
    /// ```
    /// # use embed_manifest::new_manifest;
    /// new_manifest("Company.OrgUnit.Program")
    ///     .remove_dependency("Microsoft.Windows.Common-Controls")
    /// # ;
    /// ```
    pub fn remove_dependency(mut self, name: &str) -> Self {
        self.dependent_assemblies.retain(|d| d.name != name);
        self
    }

    /// Set the “maximum version tested” based on a Windows SDK version.
    /// This compatibility setting enables the use of XAML Islands, as described in
    /// [Host a standard WinRT XAML control in a C++ desktop (Win32) app][xaml].
    ///
    /// [xaml]: https://docs.microsoft.com/en-us/windows/apps/desktop/modernize/host-standard-control-with-xaml-islands-cpp
    pub fn max_version_tested(mut self, version: MaxVersionTested) -> Self {
        self.compatibility.max_version_tested = Some(version);
        self
    }

    /// Remove the “maximum version tested” from the application compatibility.
    pub fn remove_max_version_tested(mut self) -> Self {
        self.compatibility.max_version_tested = None;
        self
    }

    /// Set the range of supported versions of Windows for application compatibility.
    /// The default value declares compatibility with every version from
    /// [Windows 7][SupportedOS::Windows7] to [Windows 10 and 11][SupportedOS::Windows10].
    pub fn supported_os<R: RangeBounds<SupportedOS>>(mut self, os_range: R) -> Self {
        use SupportedOS::*;

        self.compatibility.supported_os.clear();
        for os in [WindowsVista, Windows7, Windows8, Windows81, Windows10] {
            if os_range.contains(&os) {
                self.compatibility.supported_os.push(os);
            }
        }
        self
    }

    /// Set the code page used for single-byte Windows API, starting from Windows 10
    /// version 1903. The default setting of [UTF-8][`ActiveCodePage::Utf8`] makes Rust
    /// strings work directly with APIs like `MessageBoxA`.
    pub fn active_code_page(mut self, code_page: ActiveCodePage) -> Self {
        self.windows_settings.active_code_page = code_page;
        self
    }

    /// Configures how Windows should display this program on monitors where the
    /// graphics need scaling, whether by the application drawing its user
    /// interface at different sizes or by the Windows system rendering the graphics
    /// to a bitmap then resizing that for display.
    pub fn dpi_awareness(mut self, setting: DpiAwareness) -> Self {
        self.windows_settings.dpi_awareness = setting;
        self
    }

    /// Attempts to scale GDI primitives by the per-monitor scaling values,
    /// from Windows 10 version 1703. It seems to be best to leave this disabled.
    pub fn gdi_scaling(mut self, setting: Setting) -> Self {
        self.windows_settings.gdi_scaling = setting.enabled();
        self
    }

    /// Select the memory allocator use by the standard heap allocation APIs,
    /// including the default Rust allocator. Selecting a different algorithm
    /// may make performance and memory use better or worse, so any changes
    /// should be carefully tested.
    pub fn heap_type(mut self, setting: HeapType) -> Self {
        self.windows_settings.heap_type = setting;
        self
    }

    /// Enable paths longer than 260 characters with some wide-character Win32 APIs,
    /// when also enabled in the Windows registry. For more details, see
    /// [Maximum Path Length Limitation][1] in the Windows App Development
    /// documentation.
    ///
    /// As of Rust 1.58, the [Rust standard library bypasses this limitation][2] itself
    /// by using Unicode paths beginning with `\\?\`.
    ///
    /// [1]: https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    /// [2]: https://github.com/rust-lang/rust/pull/89174
    pub fn long_path_aware(mut self, setting: Setting) -> Self {
        self.windows_settings.long_path_aware = setting.enabled();
        self
    }

    /// Enable printer driver isolation for the application, improving security and
    /// stability when printing by loading the printer driver in a separate
    /// application. This is poorly documented, but is described in a blog post,
    /// “[Application-level Printer Driver Isolation][post]”.
    ///
    /// [post]: https://peteronprogramming.wordpress.com/2018/01/22/application-level-printer-driver-isolation/
    pub fn printer_driver_isolation(mut self, setting: Setting) -> Self {
        self.windows_settings.printer_driver_isolation = setting.enabled();
        self
    }

    /// Configure whether the application should receive mouse wheel scroll events
    /// with a minimum delta of 1, 40 or 120, as described in
    /// [Windows precision touchpad devices][touchpad].
    ///
    /// [touchpad]: https://docs.microsoft.com/en-us/windows/win32/w8cookbook/windows-precision-touchpad-devices
    pub fn scrolling_awareness(mut self, setting: ScrollingAwareness) -> Self {
        self.windows_settings.scrolling_awareness = setting;
        self
    }

    /// Allows the application to disable the filtering that normally
    /// removes UWP windows from the results of the `EnumWindows` API.
    pub fn window_filtering(mut self, setting: Setting) -> Self {
        self.windows_settings.disable_window_filtering = setting.disabled();
        self
    }

    /// Selects the authorities to execute the program with, rather than
    /// [guessing based on a filename][installer] like `setup.exe`,
    /// `update.exe` or `patch.exe`.
    ///
    /// [installer]: https://docs.microsoft.com/en-us/windows/security/identity-protection/user-account-control/how-user-account-control-works#installer-detection-technology
    pub fn requested_execution_level(mut self, level: ExecutionLevel) -> Self {
        match self.requested_execution_level {
            Some(ref mut requested_execution_level) => requested_execution_level.level = level,
            None => self.requested_execution_level = Some(RequestedExecutionLevel { level, ui_access: false }),
        }
        self
    }

    /// Allows the application to access the user interface of applications
    /// running with elevated permissions when this program does not, typically
    /// for accessibility. The program must additionally be correctly signed
    /// and installed in a trusted location like the Program Files directory.
    pub fn ui_access(mut self, access: bool) -> Self {
        match self.requested_execution_level {
            Some(ref mut requested_execution_level) => requested_execution_level.ui_access = access,
            None => {
                self.requested_execution_level = Some(RequestedExecutionLevel {
                    level: ExecutionLevel::AsInvoker,
                    ui_access: access,
                })
            }
        }
        self
    }
}

impl Display for ManifestBuilder {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        let mut w = XmlFormatter::new(f);
        w.start_document()?;
        let mut attrs = vec![("xmlns", "urn:schemas-microsoft-com:asm.v1")];
        if !self.windows_settings.is_empty() || self.requested_execution_level.is_some() {
            attrs.push(("xmlns:asmv3", "urn:schemas-microsoft-com:asm.v3"));
        }
        attrs.push(("manifestVersion", "1.0"));
        w.start_element("assembly", &attrs)?;
        if let Some(ref identity) = self.identity {
            identity.xml_to(&mut w)?;
        }
        if !self.dependent_assemblies.is_empty() {
            w.element("dependency", &[], |w| {
                for d in self.dependent_assemblies.as_slice() {
                    w.element("dependentAssembly", &[], |w| d.xml_to(w))?;
                }
                Ok(())
            })?;
        }
        if !self.compatibility.is_empty() {
            self.compatibility.xml_to(&mut w)?;
        }
        if !self.windows_settings.is_empty() {
            self.windows_settings.xml_to(&mut w)?;
        }
        if let Some(ref requested_execution_level) = self.requested_execution_level {
            requested_execution_level.xml_to(&mut w)?;
        }
        w.end_element("assembly")
    }
}

/// Identity of a side-by-side assembly dependency for the application.
#[derive(Debug)]
pub struct AssemblyIdentity {
    r#type: AssemblyType,
    name: String,
    language: Option<String>,
    processor_architecture: Option<AssemblyProcessorArchitecture>,
    version: Version,
    public_key_token: Option<PublicKeyToken>,
}

impl AssemblyIdentity {
    fn application(name: &str) -> AssemblyIdentity {
        let major = env::var("CARGO_PKG_VERSION_MAJOR").map_or(0, |s| s.parse().unwrap_or(0));
        let minor = env::var("CARGO_PKG_VERSION_MINOR").map_or(0, |s| s.parse().unwrap_or(0));
        let patch = env::var("CARGO_PKG_VERSION_PATCH").map_or(0, |s| s.parse().unwrap_or(0));
        AssemblyIdentity {
            r#type: AssemblyType::Win32,
            name: name.to_string(),
            language: None,
            processor_architecture: None,
            version: Version(major, minor, patch, 0),
            public_key_token: None,
        }
    }

    fn application_version(name: &str, major: u16, minor: u16, build: u16, revision: u16) -> AssemblyIdentity {
        AssemblyIdentity {
            r#type: AssemblyType::Win32,
            name: name.to_string(),
            language: None,
            processor_architecture: None,
            version: Version(major, minor, build, revision),
            public_key_token: None,
        }
    }

    /// Creates a new value for a [manifest dependency][ManifestBuilder::dependency],
    /// with the `version` as an array of numbers like `[1, 0, 0, 0]` for 1.0.0.0,
    /// and the public key token as a 64-bit number like `0x6595b64144ccf1df`.
    pub fn new(name: &str, version: [u16; 4], public_key_token: u64) -> AssemblyIdentity {
        AssemblyIdentity {
            r#type: AssemblyType::Win32,
            name: name.to_string(),
            language: Some("*".to_string()),
            processor_architecture: Some(AssemblyProcessorArchitecture::All),
            version: Version(version[0], version[1], version[2], version[3]),
            public_key_token: Some(PublicKeyToken(public_key_token)),
        }
    }

    /// Change the language from `"*"` to the language code in `language`.
    pub fn language(mut self, language: &str) -> Self {
        self.language = Some(language.to_string());
        self
    }

    /// Change the processor architecture from `"*"` to the architecture in `arch`.
    pub fn processor_architecture(mut self, arch: AssemblyProcessorArchitecture) -> Self {
        self.processor_architecture = Some(arch);
        self
    }

    fn xml_to(&self, w: &mut XmlFormatter) -> fmt::Result {
        let version = self.version.to_string();
        let public_key_token = self.public_key_token.as_ref().map(|token| token.to_string());

        let mut attrs: Vec<(&str, &str)> = Vec::with_capacity(6);
        if let Some(ref language) = self.language {
            attrs.push(("language", language));
        }
        attrs.push(("name", &self.name));
        if let Some(ref arch) = self.processor_architecture {
            attrs.push(("processorArchitecture", arch.as_str()))
        }
        if let Some(ref token) = public_key_token {
            attrs.push(("publicKeyToken", token));
        }
        attrs.push(("type", self.r#type.as_str()));
        attrs.push(("version", &version));
        w.empty_element("assemblyIdentity", &attrs)
    }
}

#[derive(Debug)]
struct Version(u16, u16, u16, u16);

impl fmt::Display for Version {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}.{}.{}.{}", self.0, self.1, self.2, self.3)
    }
}

#[derive(Debug)]
struct PublicKeyToken(u64);

impl fmt::Display for PublicKeyToken {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        write!(f, "{:016x}", self.0)
    }
}

/// Processor architecture for an assembly identity.
#[derive(Debug)]
#[non_exhaustive]
pub enum AssemblyProcessorArchitecture {
    /// Any processor architecture, as `"*"`.
    All,
    /// 32-bit x86 processors, as `"x86"`.
    X86,
    /// 64-bit x64 processors, as `"x64"`.
    Amd64,
    /// 32-bit ARM processors, as `"arm"`.
    Arm,
    /// 64-bit ARM processors, as `"arm64"`.
    Arm64,
}

impl AssemblyProcessorArchitecture {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::All => "*",
            Self::X86 => "x86",
            Self::Amd64 => "amd64",
            Self::Arm => "arm",
            Self::Arm64 => "arm64",
        }
    }
}

impl fmt::Display for AssemblyProcessorArchitecture {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.pad(self.as_str())
    }
}

#[derive(Debug)]
#[non_exhaustive]
enum AssemblyType {
    Win32,
}

impl AssemblyType {
    fn as_str(&self) -> &'static str {
        "win32"
    }
}

#[derive(Debug)]
struct ApplicationCompatibility {
    max_version_tested: Option<MaxVersionTested>,
    supported_os: Vec<SupportedOS>,
}

impl ApplicationCompatibility {
    fn is_empty(&self) -> bool {
        self.supported_os.is_empty()
    }

    fn xml_to(&self, w: &mut XmlFormatter) -> fmt::Result {
        w.element(
            "compatibility",
            &[("xmlns", "urn:schemas-microsoft-com:compatibility.v1")],
            |w| {
                w.element("application", &[], |w| {
                    if self.supported_os.contains(&SupportedOS::Windows10) {
                        if let Some(ref version) = self.max_version_tested {
                            w.empty_element("maxversiontested", &[("Id", version.as_str())])?;
                        }
                    }
                    for os in self.supported_os.iter() {
                        w.empty_element("supportedOS", &[("Id", os.as_str())])?
                    }
                    Ok(())
                })
            },
        )
    }
}

/// Windows build versions for [`max_version_tested()`][ManifestBuilder::max_version_tested]
/// from the [Windows SDK archive](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/).
#[derive(Debug)]
#[non_exhaustive]
pub enum MaxVersionTested {
    /// Windows 10 version 1903, with build version 10.0.18362.0.
    Windows10Version1903,
    /// Windows 10 version 2004, with build version 10.0.19041.0.
    Windows10Version2004,
    /// Windows 10 version 2104, with build version 10.0.20348.0.
    Windows10Version2104,
    /// Windows 11, with build version 10.0.22000.194.
    Windows11,
    /// Windows 11 version 22H2, with build version 10.0.22621.1.
    Windows11Version22H2,
}

impl MaxVersionTested {
    /// Return the Windows version as a string.
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Windows10Version1903 => "10.0.18362.1",
            Self::Windows10Version2004 => "10.0.19041.0",
            Self::Windows10Version2104 => "10.0.20348.0",
            Self::Windows11 => "10.0.22000.194",
            Self::Windows11Version22H2 => "10.0.22621.1",
        }
    }
}

impl Display for MaxVersionTested {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.pad(self.as_str())
    }
}

/// Operating system versions for Windows compatibility.
#[derive(Debug, Ord, PartialOrd, Eq, PartialEq)]
#[non_exhaustive]
pub enum SupportedOS {
    /// Windows Vista and Windows Server 2008.
    WindowsVista,
    /// Windows 7 and Windows Server 2008 R2.
    Windows7,
    /// Windows 8 and Windows Server 2012.
    Windows8,
    /// Windows 8.1 and Windows Server 2012 R2.
    Windows81,
    /// Windows 10 and 11, and Windows Server 2016, 2019 and 2022.
    Windows10,
}

impl SupportedOS {
    /// Returns the GUID string for the Windows version.
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::WindowsVista => "{e2011457-1546-43c5-a5fe-008deee3d3f0}",
            Self::Windows7 => "{35138b9a-5d96-4fbd-8e2d-a2440225f93a}",
            Self::Windows8 => "{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}",
            Self::Windows81 => "{1f676c76-80e1-4239-95bb-83d0f6d0da78}",
            Self::Windows10 => "{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}",
        }
    }
}

impl Display for SupportedOS {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.pad(self.as_str())
    }
}

static WS2005: (&str, &str) = ("xmlns", "http://schemas.microsoft.com/SMI/2005/WindowsSettings");
static WS2011: (&str, &str) = ("xmlns", "http://schemas.microsoft.com/SMI/2011/WindowsSettings");
static WS2013: (&str, &str) = ("xmlns", "http://schemas.microsoft.com/SMI/2013/WindowsSettings");
static WS2016: (&str, &str) = ("xmlns", "http://schemas.microsoft.com/SMI/2016/WindowsSettings");
static WS2017: (&str, &str) = ("xmlns", "http://schemas.microsoft.com/SMI/2017/WindowsSettings");
static WS2019: (&str, &str) = ("xmlns", "http://schemas.microsoft.com/SMI/2019/WindowsSettings");
static WS2020: (&str, &str) = ("xmlns", "http://schemas.microsoft.com/SMI/2020/WindowsSettings");

#[derive(Debug)]
struct WindowsSettings {
    active_code_page: ActiveCodePage,
    disable_window_filtering: bool,
    dpi_awareness: DpiAwareness,
    gdi_scaling: bool,
    heap_type: HeapType,
    long_path_aware: bool,
    printer_driver_isolation: bool,
    scrolling_awareness: ScrollingAwareness,
}

impl WindowsSettings {
    fn new() -> Self {
        Self {
            active_code_page: ActiveCodePage::Utf8,
            disable_window_filtering: false,
            dpi_awareness: DpiAwareness::PerMonitorV2Only,
            gdi_scaling: false,
            heap_type: HeapType::LowFragmentationHeap,
            long_path_aware: true,
            printer_driver_isolation: true,
            scrolling_awareness: ScrollingAwareness::UltraHighResolution,
        }
    }

    fn empty() -> Self {
        Self {
            active_code_page: ActiveCodePage::System,
            disable_window_filtering: false,
            dpi_awareness: DpiAwareness::UnawareByDefault,
            gdi_scaling: false,
            heap_type: HeapType::LowFragmentationHeap,
            long_path_aware: false,
            printer_driver_isolation: false,
            scrolling_awareness: ScrollingAwareness::UltraHighResolution,
        }
    }

    fn is_empty(&self) -> bool {
        matches!(
            self,
            Self {
                active_code_page: ActiveCodePage::System,
                disable_window_filtering: false,
                dpi_awareness: DpiAwareness::UnawareByDefault,
                gdi_scaling: false,
                heap_type: HeapType::LowFragmentationHeap,
                long_path_aware: false,
                printer_driver_isolation: false,
                scrolling_awareness: ScrollingAwareness::UltraHighResolution,
            }
        )
    }

    fn xml_to(&self, w: &mut XmlFormatter) -> fmt::Result {
        w.element("asmv3:application", &[], |w| {
            w.element("asmv3:windowsSettings", &[], |w| {
                self.active_code_page.xml_to(w)?;
                if self.disable_window_filtering {
                    w.element("disableWindowFiltering", &[WS2011], |w| w.text("true"))?;
                }
                self.dpi_awareness.xml_to(w)?;
                if self.gdi_scaling {
                    w.element("gdiScaling", &[WS2017], |w| w.text("true"))?;
                }
                if matches!(self.heap_type, HeapType::SegmentHeap) {
                    w.element("heapType", &[WS2020], |w| w.text("SegmentHeap"))?;
                }
                if self.long_path_aware {
                    w.element("longPathAware", &[WS2016], |w| w.text("true"))?;
                }
                if self.printer_driver_isolation {
                    w.element("printerDriverIsolation", &[WS2011], |w| w.text("true"))?;
                }
                self.scrolling_awareness.xml_to(w)
            })
        })
    }
}

/// Configure whether a Windows setting is enabled or disabled, avoiding confusion
/// over which of these options `true` and `false` represent.
#[derive(Debug)]
pub enum Setting {
    Disabled = 0,
    Enabled = 1,
}

impl Setting {
    /// Returns `true` if the setting should be disabled.
    fn disabled(&self) -> bool {
        matches!(self, Setting::Disabled)
    }

    /// Returns `true` if the setting should be enabled.
    fn enabled(&self) -> bool {
        matches!(self, Setting::Enabled)
    }
}

/// The code page used by single-byte APIs in the program.
#[derive(Debug)]
#[non_exhaustive]
pub enum ActiveCodePage {
    /// Use the code page from the configured system locale, or UTF-8 if “Use Unicode UTF-8
    /// for worldwide language support” is configured.
    System,
    /// Use UTF-8 from Windows 10 version 1903 and on Windows 11.
    Utf8,
    /// Use the code page from the configured system locale, even when “Use Unicode UTF-8
    /// for worldwide language support” is configured.
    Legacy,
    /// Use the code page from the configured system locale on Windows 10, or from this
    /// locale on Windows 11.
    Locale(String),
}

impl ActiveCodePage {
    pub fn as_str(&self) -> &str {
        match self {
            Self::System => "",
            Self::Utf8 => "UTF-8",
            Self::Legacy => "Legacy",
            Self::Locale(s) => s,
        }
    }

    fn xml_to(&self, w: &mut XmlFormatter) -> fmt::Result {
        match self {
            Self::System => Ok(()),
            _ => w.element("activeCodePage", &[WS2019], |w| w.text(self.as_str())),
        }
    }
}

impl Display for ActiveCodePage {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.pad(self.as_str())
    }
}

/// Options for how Windows will handle drawing on monitors when the graphics
/// need scaling to display at the correct size.
///
/// See [High DPI Desktop Application Development on Windows][dpi] for more details
/// about the impact of these options.
///
/// [dpi]: https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
#[derive(Debug)]
#[non_exhaustive]
pub enum DpiAwareness {
    /// DPI awareness is not configured, so Windows will scale the application unless
    /// changed via the `SetProcessDpiAware` or `SetProcessDpiAwareness` API.
    UnawareByDefault,
    /// DPI awareness is not configured, with Windows 8.1, 10 and 11 not able
    /// to change the setting via API.
    Unaware,
    /// The program uses the system DPI, or the DPI of the monitor they start on if
    /// “Fix scaling for apps” is enabled. If the DPI does not match the current
    /// monitor then Windows will scale the application.
    System,
    /// The program uses the system DPI on Windows Vista, 7 and 8, and version 1 of
    /// per-monitor DPI awareness on Windows 8.1, 10 and 11. Using version 1 of the
    /// API is not recommended.
    PerMonitor,
    /// The program uses the system DPI on Windows Vista, 7 and 8, version 1 of
    /// per-monitor DPI awareness on Windows 8.1 and Windows 10 version 1507 and 1511,
    /// and version 2 of per-monitor DPI awareness from Windows 10 version 1607.
    PerMonitorV2,
    /// The program uses the system DPI on Windows Vista, 7, 8, 8.1 and Windows 10
    /// version 1507 and 1511, and version 2 of per-monitor DPI awareness from
    /// Windows 10 version 1607.
    PerMonitorV2Only,
}

impl DpiAwareness {
    fn xml_to(&self, w: &mut XmlFormatter) -> fmt::Result {
        let settings = match self {
            Self::UnawareByDefault => (None, None),
            Self::Unaware => (Some("false"), None),
            Self::System => (Some("true"), None),
            Self::PerMonitor => (Some("true/pm"), None),
            Self::PerMonitorV2 => (Some("true/pm"), Some("permonitorv2,permonitor")),
            Self::PerMonitorV2Only => (None, Some("permonitorv2")),
        };
        if let Some(dpi_aware) = settings.0 {
            w.element("dpiAware", &[WS2005], |w| w.text(dpi_aware))?;
        }
        if let Some(dpi_awareness) = settings.1 {
            w.element("dpiAwareness", &[WS2016], |w| w.text(dpi_awareness))?;
        }
        Ok(())
    }
}

/// The heap type for the default memory allocator.
#[derive(Debug)]
#[non_exhaustive]
pub enum HeapType {
    /// The default heap type since Windows Vista.
    LowFragmentationHeap,
    /// The modern segment heap, which may reduce total memory allocation in some programs.
    /// This is supported since Windows 10 version 2004. See
    /// [Improving Memory Usage in Microsoft Edge][edge].
    ///
    /// [edge]: https://blogs.windows.com/msedgedev/2020/06/17/improving-memory-usage/
    SegmentHeap,
}

/// Whether the application supports scroll wheel events with a minimum delta
/// of 1, 40 or 120.
#[derive(Debug)]
#[non_exhaustive]
pub enum ScrollingAwareness {
    /// The application can only handle scroll wheel events with the original delta of 120.
    LowResolution,
    /// The application can handle high precision scroll wheel events with a delta of 40.
    HighResolution,
    /// The application can handle ultra high precision scroll wheel events with a delta as low as 1.
    UltraHighResolution,
}

impl ScrollingAwareness {
    fn xml_to(&self, w: &mut XmlFormatter) -> fmt::Result {
        match self {
            Self::LowResolution => w.element("ultraHighResolutionScrollingAware", &[WS2013], |w| w.text("false")),
            Self::HighResolution => w.element("highResolutionScrollingAware", &[WS2013], |w| w.text("true")),
            Self::UltraHighResolution => Ok(()),
        }
    }
}

#[derive(Debug)]
struct RequestedExecutionLevel {
    level: ExecutionLevel,
    ui_access: bool,
}

impl RequestedExecutionLevel {
    fn xml_to(&self, w: &mut XmlFormatter) -> fmt::Result {
        w.element("asmv3:trustInfo", &[], |w| {
            w.element("asmv3:security", &[], |w| {
                w.element("asmv3:requestedPrivileges", &[], |w| {
                    w.empty_element(
                        "asmv3:requestedExecutionLevel",
                        &[
                            ("level", self.level.as_str()),
                            ("uiAccess", if self.ui_access { "true" } else { "false" }),
                        ],
                    )
                })
            })
        })
    }
}

/// The requested execution level for the application when started.
///
/// The behaviour of each option is described in
/// [Designing UAC Applications for Windows Vista Step 6: Create and Embed an Application Manifest][step6].
///
/// [step6]: https://msdn.microsoft.com/en-us/library/bb756929.aspx
#[derive(Debug)]
pub enum ExecutionLevel {
    /// The application will always run with the same authorities as the program invoking it.
    AsInvoker,
    /// The program will run without special authorities for a standard user, but will try to
    /// run with administrator authority if the user is an administrator. This is rarely used.
    HighestAvailable,
    /// The application will run as an administrator, prompting for elevation if necessary.
    RequireAdministrator,
}

impl ExecutionLevel {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::AsInvoker => "asInvoker",
            Self::HighestAvailable => "highestAvailable",
            Self::RequireAdministrator => "requireAdministrator",
        }
    }
}

impl Display for ExecutionLevel {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        f.pad(self.as_str())
    }
}
