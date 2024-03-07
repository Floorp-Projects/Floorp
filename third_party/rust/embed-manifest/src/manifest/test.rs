use super::{ExecutionLevel, SupportedOS};
use crate::{empty_manifest, new_manifest};

fn enc(s: &str) -> String {
    let mut buf = String::with_capacity(1024);
    buf.push('\u{feff}');
    for l in s.lines() {
        buf.push_str(l);
        buf.push_str("\r\n");
    }
    buf.truncate(buf.len() - 2);
    buf
}

fn encp(s: &str) -> String {
    s.replace("\n", "\r\n")
}

#[test]
fn empty_manifest_canonical() {
    let builder = empty_manifest();
    assert_eq!(
        format!("{}", builder),
        enc(r#"<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"></assembly>"#)
    );
}

#[test]
fn empty_manifest_pretty() {
    let builder = empty_manifest();
    assert_eq!(
        format!("{:#}", builder),
        encp(
            r#"<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"/>"#
        )
    );
}

#[test]
fn only_execution_level() {
    let builder = empty_manifest().requested_execution_level(ExecutionLevel::AsInvoker);
    assert_eq!(
        format!("{:#}", builder),
        encp(
            r#"<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" xmlns:asmv3="urn:schemas-microsoft-com:asm.v3" manifestVersion="1.0">
    <asmv3:trustInfo>
        <asmv3:security>
            <asmv3:requestedPrivileges>
                <asmv3:requestedExecutionLevel level="asInvoker" uiAccess="false"/>
            </asmv3:requestedPrivileges>
        </asmv3:security>
    </asmv3:trustInfo>
</assembly>"#
        )
    );
}

#[test]
fn only_windows10() {
    let builder = empty_manifest().supported_os(SupportedOS::Windows10..);
    assert_eq!(
        format!("{:#}", builder),
        encp(
            r#"<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
    <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
        <application>
            <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
        </application>
    </compatibility>
</assembly>"#
        )
    );
}

#[test]
fn no_comctl32_6() {
    let builder = new_manifest("Company.OrgUnit.Program")
        .version(1, 0, 0, 2)
        .remove_dependency("Microsoft.Windows.Common-Controls");
    assert_eq!(
        format!("{:#}", builder),
        encp(
            r#"<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" xmlns:asmv3="urn:schemas-microsoft-com:asm.v3" manifestVersion="1.0">
    <assemblyIdentity name="Company.OrgUnit.Program" type="win32" version="1.0.0.2"/>
    <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
        <application>
            <maxversiontested Id="10.0.18362.1"/>
            <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}"/>
            <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}"/>
            <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>
            <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
        </application>
    </compatibility>
    <asmv3:application>
        <asmv3:windowsSettings>
            <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
            <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">permonitorv2</dpiAwareness>
            <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
            <printerDriverIsolation xmlns="http://schemas.microsoft.com/SMI/2011/WindowsSettings">true</printerDriverIsolation>
        </asmv3:windowsSettings>
    </asmv3:application>
    <asmv3:trustInfo>
        <asmv3:security>
            <asmv3:requestedPrivileges>
                <asmv3:requestedExecutionLevel level="asInvoker" uiAccess="false"/>
            </asmv3:requestedPrivileges>
        </asmv3:security>
    </asmv3:trustInfo>
</assembly>"#
        )
    );
}
