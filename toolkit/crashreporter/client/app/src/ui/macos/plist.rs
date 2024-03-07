/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The embedded Info.plist file.

const DATA: &[u8] = br#"<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>English</string>
	<key>CFBundleDisplayName</key>
	<string>Crash Reporter</string>
	<key>CFBundleExecutable</key>
	<string>crashreporter</string>
	<key>CFBundleIdentifier</key>
	<string>org.mozilla.crashreporter</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>Crash Reporter</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleVersion</key>
	<string>1.0</string>
	<key>LSHasLocalizedDisplayName</key>
	<true/>
	<key>NSRequiresAquaSystemAppearance</key>
	<false/>
	<key>NSPrincipalClass</key>
	<string>NSApplication</string>
</dict>
</plist>"#;

const N: usize = DATA.len();

const PTR: *const [u8; N] = DATA.as_ptr() as *const [u8; N];

#[used]
#[link_section = "__TEXT,__info_plist"]
// # Safety
// The array pointer is created from `DATA` (a slice pointer) with `DATA.len()` as the length.
static PLIST: [u8; N] = unsafe { *PTR };
