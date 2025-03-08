#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import glob
import logging
import os
import platform
import plistlib
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml

def setup_logging(verbose):
    """Set up logging configuration"""
    logging_level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=logging_level,
        format='%(asctime)s - %(levelname)s - %(message)s'
    )
    return logging.getLogger(__name__)

def check_platform():
    """Verify that we're running on Linux"""
    if platform.system() != "Linux":
        logging.error("This script must be run on Linux")
        sys.exit(1)

def check_rcodesign():
    """Verify that rcodesign is installed"""
    if not subprocess.run(["which", "rcodesign"], capture_output=True).returncode == 0:
        logging.error("rcodesign is not installed. Please install it first.")
        sys.exit(1)

def auto_detect_channel(app_path):
    """Detect the channel (nightly, release, etc.) from the app bundle"""
    NIGHTLY_BUNDLEID = "org.mozilla.nightly"
    DEVEDITION_BUNDLEID = "org.mozilla.firefoxdeveloperedition"
    RELEASE_BUNDLEID = "org.mozilla.firefox"

    info_plist = os.path.join(app_path, "Contents/Info.plist")

    if not os.path.exists(info_plist):
        logging.error(f"Info.plist not found at {info_plist}")
        sys.exit(1)

    with open(info_plist, 'rb') as f:
        plist_data = plistlib.load(f)
        bundle_id = plist_data.get('CFBundleIdentifier')

        if bundle_id == NIGHTLY_BUNDLEID:
            return "nightly"
        elif bundle_id == DEVEDITION_BUNDLEID:
            return "devedition"
        elif bundle_id == RELEASE_BUNDLEID:
            return "release"
        else:
            logging.error(f"Unknown bundle ID: {bundle_id}")
            sys.exit(1)

def strip_restricted_entitlements(plist_file):
    """Remove restricted entitlements from the plist file"""
    restricted_entitlements = [
        "com.apple.developer.web-browser.public-key-credential",
        "com.apple.application-identifier",
    ]

    with open(plist_file, "rb") as f:
        plist_data = plistlib.load(f, fmt=plistlib.FMT_XML)
        for entitlement in restricted_entitlements:
            if entitlement in plist_data:
                del plist_data[entitlement]

    _, temp_file_path = tempfile.mkstemp(prefix="linux-macos-sign.")
    with open(temp_file_path, "wb") as f:
        plistlib.dump(plist_data, f)

    return temp_file_path

def verify_signature(logger, app_path, verbose=False):
    """Verify the signature of the app bundle using rcodesign"""
    # Find main executable
    app_name = os.path.basename(app_path)
    if app_name.endswith('.app'):
        app_name = app_name[:-4]

    main_executable = os.path.join(app_path, "Contents/MacOS", app_name)

    if not os.path.exists(main_executable):
        macos_dir = os.path.join(app_path, "Contents/MacOS")
        if os.path.isdir(macos_dir):
            executables = [f for f in os.listdir(macos_dir) if os.path.isfile(os.path.join(macos_dir, f))]
            if executables:
                main_executable = os.path.join(macos_dir, executables[0])

    if not os.path.exists(main_executable):
        logger.error(f"Could not find main executable in {app_path}")
        sys.exit(1)

    # Verify main executable
    logger.info(f"Verifying main executable: {main_executable}")
    try:
        cmd = ["rcodesign", "verify", main_executable, "--verbose"]
        subprocess.run(cmd, check=True, capture_output=not verbose)
    except subprocess.CalledProcessError as e:
        logger.error(f"Verification failed with exit code {e.returncode}")
        sys.exit(e.returncode)

    # Verify frameworks
    frameworks_dir = os.path.join(app_path, "Contents/Frameworks")
    if os.path.isdir(frameworks_dir):
        logger.info(f"Verifying frameworks in {frameworks_dir}")
        for framework in ["XUL.framework"]:
            framework_path = os.path.join(frameworks_dir, framework)
            if os.path.exists(framework_path):
                versions_dir = os.path.join(framework_path, "Versions")
                if os.path.isdir(versions_dir):
                    for version in os.listdir(versions_dir):
                        if version != "Current":
                            bin_path = os.path.join(versions_dir, version, framework[:-10])
                            if os.path.exists(bin_path) and os.path.isfile(bin_path):
                                try:
                                    cmd = ["rcodesign", "verify", bin_path, "--verbose"]
                                    subprocess.run(cmd, check=True, capture_output=not verbose)
                                    logger.info(f"Successfully verified {bin_path}")
                                except subprocess.CalledProcessError as e:
                                    logger.warning(f"Verification of {bin_path} failed with exit code {e.returncode}")

def sign_app(app_path, channel, entitlements_type, p12_file=None, p12_password_file=None, verbose=False):
    """Sign the macOS app bundle using rcodesign"""
    logger = setup_logging(verbose)

    # Initial checks
    check_platform()
    check_rcodesign()

    # Normalize app path (remove trailing slashes)
    app_path = app_path.rstrip('/')

    if not os.path.isdir(app_path):
        logger.error(f"App bundle not found: {app_path}")
        sys.exit(1)

    # Remove existing signatures
    # logger.info("Removing existing signatures...")
    # subprocess.run(["rcodesign", "remove-signature", app_path, "--recursive"], check=True)

    # Load signing configuration
    config_path = os.path.join(os.getcwd(), "taskcluster/config.yml")
    if not os.path.exists(config_path):
        logger.error(f"Config file not found: {config_path}")
        sys.exit(1)

    with open(config_path, 'r') as f:
        config = yaml.safe_load(f)

    # Get signing configuration based on entitlements type
    entitlements_key = "production" if entitlements_type in ["production", "production-without-restricted"] else "default"
    signing_groups = config["mac-signing"]["hardened-sign-config"]["by-hardened-signing-type"][entitlements_key]

    # Additional executables that need hardened runtime
    additional_executables = [
        "Contents/MacOS/updater.app/Contents/MacOS/org.mozilla.updater",
        "Contents/Library/LaunchServices/org.mozilla.updater",
        "Contents/MacOS/nmhproxy",
        "Contents/MacOS/pingsender",
        "Contents/MacOS/floorp",
    ]

    binary_paths = []
    entitlements_map = {}
    runtime_flags = set()
    temp_files = []

    # Process signing groups
    for group in signing_groups:
        if "entitlements" in group:
            entitlement_file = None

            # Get entitlements file path
            if isinstance(group["entitlements"], str):
                entitlement_file = group["entitlements"]
            elif isinstance(group["entitlements"], dict):
                if channel == "nightly":
                    entitlement_file = group["entitlements"]["by-build-platform"]["default"]["by-project"]["mozilla-central"]
                elif channel == "devedition":
                    entitlement_file = group["entitlements"]["by-build-platform"][".*devedition.*"]
                elif channel in ["release", "beta"]:
                    entitlement_file = group["entitlements"]["by-build-platform"]["default"]["by-project"]["default"]
                else:
                    raise ValueError(f"Unexpected channel: {channel}")

            # Strip restricted entitlements if needed
            if entitlements_type == "production-without-restricted":
                entitlement_file = strip_restricted_entitlements(entitlement_file)
                temp_files.append(entitlement_file)

            # Process paths for this signing group
            for pathglob in group["globs"]:
                matched_paths = glob.glob(os.path.join(app_path, pathglob.strip("/")), recursive=True)

                # Add additional executables if this is the root signing group
                if pathglob == "/":
                    for exe in additional_executables:
                        exe_path = os.path.join(app_path, exe)
                        if os.path.exists(exe_path):
                            matched_paths.append(exe_path)

                for path in matched_paths:
                    binary_paths.append(path)
                    if entitlement_file:
                        rel_path = os.path.relpath(path, app_path)
                        entitlements_map[rel_path] = entitlement_file
                    if "runtime" in group and group["runtime"]:
                        runtime_flags.add(os.path.relpath(path, app_path))

    # Prepare rcodesign command
    cmd = ["rcodesign", "sign"]
    
    # Add signing credentials if provided
    if p12_file and p12_password_file:
        cmd.extend(["--p12-file", p12_file, "--p12-password-file", p12_password_file])

    # Add hardened runtime flags
    for path in runtime_flags:
        cmd.extend(["--code-signature-flags", f"{path}:runtime"])

    # Add entitlements
    for path, entitlement in entitlements_map.items():
        cmd.extend(["--entitlements-xml-path", f"{path}:{entitlement}"])

    # Add the app bundle as the main input path
    cmd.append(app_path)

    # Execute signing
    logger.info("Signing app bundle...")
    logger.debug(f"Executing command: {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True, capture_output=not verbose)
    except subprocess.CalledProcessError as e:
        logger.error(f"Signing failed with exit code {e.returncode}")
        if verbose:
            logger.error(f"Command output: {e.output.decode('utf-8')}")
        sys.exit(e.returncode)

    # Clean up temporary files
    for temp_file in temp_files:
        os.unlink(temp_file)

    # Verify signature
    logger.info("Verifying signatures...")
    verify_signature(logger, app_path, verbose)
    logger.info("Signing completed successfully!")

def main():
    parser = argparse.ArgumentParser(description="Sign macOS Firefox/Floorp app bundle on Linux using rcodesign")
    parser.add_argument("app_path", help="Path to the .app bundle to sign")
    parser.add_argument("--channel", choices=["auto", "nightly", "devedition", "beta", "release"],
                      default="auto", help="Channel of the build (default: auto-detect)")
    parser.add_argument("--entitlements", choices=["developer", "production", "production-without-restricted"],
                      default="developer", help="Type of entitlements to use (default: developer)")
    parser.add_argument("--p12-file", help="Path to PKCS#12 certificate file")
    parser.add_argument("--p12-password-file", help="Path to PKCS#12 password file")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")

    args = parser.parse_args()

    # Auto-detect channel if needed
    if args.channel == "auto":
        args.channel = auto_detect_channel(args.app_path)

    # Validate p12 file arguments
    if bool(args.p12_file) != bool(args.p12_password_file):
        parser.error("Both --p12-file and --p12-password-file must be provided together")

    sign_app(
        args.app_path,
        args.channel,
        args.entitlements,
        args.p12_file,
        args.p12_password_file,
        args.verbose
    )

if __name__ == "__main__":
    main()