#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
macOS App Signing Script for Linux - Production Version

This script signs a macOS application bundle (.app) using rcodesign on Linux.

Requirements:
- rcodesign must be installed (https://github.com/indygreg/apple-platform-rs)
- P12 certificate and password file

Usage:
  python linux_sign_macos_app.py APP_PATH P12_FILE P12_PASSWORD_FILE
"""

import logging
import os
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile
import glob

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger("linux-macos-sign")

KNOWN_EXECUTABLES = [
    "Contents/Library/LaunchServices/org.mozilla.updater",
    "Contents/MacOS/nmhproxy",
    "Contents/MacOS/pingsender",
    "Contents/MacOS/media-plugin-helper.app/Contents/MacOS/Floorp Daylight Media Plugin Helper",
]

def check_platform():
    """Ensure running on Linux"""
    if platform.system() != "Linux":
        logger.error("This script must run on Linux")
        sys.exit(1)

def check_rcodesign():
    """Check if rcodesign is available"""
    if shutil.which("rcodesign") is None:
        logger.error("rcodesign not found. Install from https://github.com/indygreg/apple-platform-rs")
        sys.exit(1)

def run_command(cmd, verbose=False):
    """Execute a command with error handling"""
    cmd_str = " ".join(cmd)
    logger.debug(f"Running: {cmd_str}")

    try:
        result = subprocess.run(cmd, capture_output=not verbose, text=True, check=True)
        return result
    except subprocess.CalledProcessError as e:
        logger.error(f"Command failed: {cmd_str}")
        if not verbose:
            logger.error(f"stdout: {e.stdout}")
            logger.error(f"stderr: {e.stderr}")
        sys.exit(e.returncode)

def get_app_id(app_path):
    """Extract bundle ID from Info.plist"""
    info_plist = os.path.join(app_path, "Contents", "Info.plist")

    if not os.path.exists(info_plist):
        logger.warning(f"Info.plist not found at {info_plist}")
        return "org.mozilla.firefox"

    try:
        with open(info_plist, 'rb') as f:
            data = plistlib.load(f)
            return data.get("CFBundleIdentifier", "org.mozilla.firefox")
    except Exception as e:
        logger.warning(f"Error reading Info.plist: {e}")
        return "org.mozilla.firefox"

def create_entitlements(app_id):
    """Create production entitlements file"""
    entitlements = {
        "com.apple.security.app-sandbox": True,
        "com.apple.security.cs.allow-jit": True,
        "com.apple.security.cs.allow-unsigned-executable-memory": True,
        "com.apple.security.cs.disable-library-validation": True,
        "com.apple.security.network.client": True,
        "com.apple.security.network.server": True,
        "com.apple.application-identifier": app_id,
        "com.apple.developer.web-browser.public-key-credential": True,
        "com.apple.security.cs.debugger": True,
    }

    fd, temp_path = tempfile.mkstemp(prefix="entitlements-", suffix=".plist")
    with os.fdopen(fd, 'wb') as f:
        plistlib.dump(entitlements, f)

    return temp_path

def find_nested_apps(app_path):
    """Find all nested .app bundles within the main app"""
    nested_apps = []

    macos_dir = os.path.join(app_path, "Contents/MacOS")
    if os.path.exists(macos_dir):
        for item in os.listdir(macos_dir):
            item_path = os.path.join(macos_dir, item)
            if os.path.isdir(item_path) and item.endswith('.app'):
                nested_apps.append(item_path)

    for root, dirs, files in os.walk(app_path):
        for dir_name in dirs:
            if dir_name.endswith('.app') and os.path.join(root, dir_name) not in nested_apps:
                nested_apps.append(os.path.join(root, dir_name))

    logger.info(f"Found {len(nested_apps)} nested application bundles")
    return nested_apps

def find_all_executables(app_path):
    """Find all executable files within the app bundle"""
    executables = []

    macos_dir = os.path.join(app_path, "Contents/MacOS")
    if os.path.exists(macos_dir):
        for item in os.listdir(macos_dir):
            item_path = os.path.join(macos_dir, item)
            if os.path.isfile(item_path) and os.access(item_path, os.X_OK):
                executables.append(item_path)

    launch_services_dir = os.path.join(app_path, "Contents/Library/LaunchServices")
    if os.path.exists(launch_services_dir):
        for item in os.listdir(launch_services_dir):
            item_path = os.path.join(launch_services_dir, item)
            if os.path.isfile(item_path) and os.access(item_path, os.X_OK):
                executables.append(item_path)

    for known_exe in KNOWN_EXECUTABLES:
        exe_path = os.path.join(app_path, known_exe)
        if os.path.exists(exe_path) and exe_path not in executables:
            executables.append(exe_path)

    logger.info(f"Found {len(executables)} executable files")
    return executables

def prepare_signing_command(app_path, p12_file, p12_password_file, entitlements_file):
    """Prepare the base signing command"""
    cmd = [
        "rcodesign", "sign",
        "--p12-file", p12_file,
        "--p12-password-file", p12_password_file,
        "--entitlements-xml-path", entitlements_file,
        "--code-signature-flags", "runtime",
    ]

    return cmd

def add_nested_app_arguments(cmd, app_path, nested_app_path, entitlements_file):
    """Add arguments for a nested app"""
    relative_path = os.path.relpath(nested_app_path, app_path)

    # For each nested app bundle, add the runtime flag and entitlements
    cmd.append("--code-signature-flags")
    cmd.append(f"{relative_path}:runtime")
    cmd.append("--entitlements-xml-path")
    cmd.append(f"{relative_path}:{entitlements_file}")

def add_executable_arguments(cmd, app_path, executable_path, entitlements_file):
    """Add arguments for individual executables"""
    relative_path = os.path.relpath(executable_path, app_path)

    cmd.append("--code-signature-flags")
    cmd.append(f"{relative_path}:runtime")
    cmd.append("--entitlements-xml-path")
    cmd.append(f"{relative_path}:{entitlements_file}")

def sign_app(app_path, p12_file, p12_password_file, entitlements_file):
    """Sign the app with nested bundles and all executables properly handled"""
    logger.info(f"Preparing to sign {app_path}")

    nested_apps = find_nested_apps(app_path)
    logger.info(f"Found {len(nested_apps)} nested app bundles to sign")

    executables = find_all_executables(app_path)
    logger.info(f"Found {len(executables)} executables to sign")

    cmd = prepare_signing_command(app_path, p12_file, p12_password_file, entitlements_file)

    for nested_app in nested_apps:
        logger.info(f"Adding signing configuration for nested app: {nested_app}")
        add_nested_app_arguments(cmd, app_path, nested_app, entitlements_file)

        nested_executables = find_all_executables(nested_app)
        for nested_exe in nested_executables:
            logger.info(f"Adding signing configuration for nested executable: {nested_exe}")
            add_executable_arguments(cmd, app_path, nested_exe, entitlements_file)

    for executable in executables:
        logger.info(f"Adding signing configuration for executable: {executable}")
        add_executable_arguments(cmd, app_path, executable, entitlements_file)

    cmd.append(app_path)

    logger.info("Executing signing command")
    run_command(cmd, verbose=True)

def verify_signature(app_path):
    """Verify the signature"""
    logger.info("Verifying signatures")

    app_name = os.path.basename(app_path)
    if app_name.endswith('.app'):
        app_name = app_name[:-4]

    main_executable = os.path.join(app_path, "Contents/MacOS", app_name)

    if not os.path.exists(main_executable):
        macos_dir = os.path.join(app_path, "Contents/MacOS")
        if os.path.isdir(macos_dir):
            executables = [f for f in os.listdir(macos_dir)
                          if os.path.isfile(os.path.join(macos_dir, f))]
            if executables:
                main_executable = os.path.join(macos_dir, executables[0])

    if not os.path.exists(main_executable):
        logger.error(f"Main executable not found in {app_path}")
        sys.exit(1)

    logger.info(f"Verifying main executable: {main_executable}")
    cmd = ["rcodesign", "verify", main_executable, "--verbose"]
    run_command(cmd)

    for known_exe in KNOWN_EXECUTABLES:
        exe_path = os.path.join(app_path, known_exe)
        if os.path.exists(exe_path):
            logger.info(f"👀 Verifying known executable: {exe_path}")
            cmd = ["rcodesign", "verify", exe_path, "--verbose"]
            try:
                run_command(cmd)
                logger.info(f"👌 Verification successful for {exe_path}")
            except Exception as e:
                logger.error(f"❌ Failed to verify {exe_path}: {e}")

    nested_apps = find_nested_apps(app_path)
    for nested_app in nested_apps:
        nested_app_name = os.path.basename(nested_app)
        if nested_app_name.endswith('.app'):
            nested_app_name = nested_app_name[:-4]

        nested_executable = os.path.join(nested_app, "Contents/MacOS", nested_app_name)
        if not os.path.exists(nested_executable):
            macos_dir = os.path.join(nested_app, "Contents/MacOS")
            if os.path.isdir(macos_dir):
                executables = [f for f in os.listdir(macos_dir)
                              if os.path.isfile(os.path.join(macos_dir, f))]
                if executables:
                    nested_executable = os.path.join(macos_dir, executables[0])

        if os.path.exists(nested_executable):
            logger.info(f"👀 verifying nested executable: {nested_executable}")
            cmd = ["rcodesign", "verify", nested_executable, "--verbose"]
            try:
                run_command(cmd)
                logger.info(f"👌 Verification successful for {nested_executable}")
            except Exception as e:
                logger.error(f"❌ Failed to verify {nested_executable}: {e}")

    logger.info("ℹ️ Signature verification completed")

def main():
    if len(sys.argv) != 4:
        logger.error("Usage: python linux_sign_macos_app.py APP_PATH P12_FILE P12_PASSWORD_FILE")
        sys.exit(1)

    app_path = os.path.realpath(sys.argv[1])
    p12_file = sys.argv[2]
    p12_password_file = sys.argv[3]

    if not os.path.isdir(app_path):
        logger.error(f"🍎 App path is not a directory: {app_path}")
        sys.exit(1)

    if not os.path.isfile(p12_file):
        logger.error(f"🔓 P12 file not found: {p12_file}")
        sys.exit(1)

    if not os.path.isfile(p12_password_file):
        logger.error(f"📃 P12 password file not found: {p12_password_file}")
        sys.exit(1)

    check_platform()
    check_rcodesign()

    temp_files = []
    try:
        app_id = get_app_id(app_path)
        logger.info(f"App ID: {app_id}")

        entitlements_file = create_entitlements(app_id)
        temp_files.append(entitlements_file)
        logger.info(f"Created entitlements file: {entitlements_file}")

        sign_app(app_path, p12_file, p12_password_file, entitlements_file)
        verify_signature(app_path)

        logger.info(f"Successfully signed {app_path}")

    finally:
        for temp_file in temp_files:
            if os.path.exists(temp_file):
                os.remove(temp_file)

if __name__ == "__main__":
    main()
