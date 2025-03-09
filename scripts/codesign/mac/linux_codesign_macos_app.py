#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import glob
import logging
import os
import os.path
import platform
import plistlib
import subprocess
import sys
import tempfile
import shutil
import argparse
from pathlib import Path

import yaml

def setup_logger(verbose):
    """Set up logging configuration"""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format='%(levelname)s: %(message)s'
    )
    return logging.getLogger(__name__)

class Context:
    """Simple context class to mimic mach command context"""
    def __init__(self, logger):
        self.logger = logger
        self.topsrcdir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

    def log(self, level, component, params, message):
        """Format and log a message with parameters"""
        formatted_message = message.format(**params) if params else message
        self.logger.log(level, f"[{component}] {formatted_message}")

    def _set_log_level(self, verbose):
        """Set the logging level based on verbosity"""
        self.logger.setLevel(logging.DEBUG if verbose else logging.INFO)

def parse_args():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Sign a built and packaged Firefox bundle on macOS or Linux (using rcodesign)."
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        default=False,
        help="Verbose output including the commands executed."
    )
    parser.add_argument(
        "-a", "--app-path",
        required=True,
        help="Path to the .app bundle to sign. This can not be the .app built "
             "in the object dir because it contains symlinks and is unbundled. Use "
             "an app generated with ./mach build package."
    )
    parser.add_argument(
        "-s", "--signing-identity",
        default=None,
        help="The codesigning identity to be used when signing with the macOS "
             "native codesign tool. By default ad-hoc self-signing will be used."
    )
    parser.add_argument(
        "-e", "--entitlements",
        default="developer",
        choices=["developer", "production", "production-without-restricted"],
        help="Whether to sign the build for development or production use."
    )
    parser.add_argument(
        "-c", "--channel",
        default="auto",
        choices=["auto", "nightly", "devedition", "beta", "release"],
        help="Which channel build is being signed."
    )
    rcodesign_group = parser.add_argument_group("rcodesign")
    rcodesign_group.add_argument(
        "-r", "--use-rcodesign",
        action="store_true",
        default=False,
        help="Enables signing with rcodesign instead of codesign."
    )
    rcodesign_group.add_argument(
        "-f", "--rcodesign-p12-file",
        default=None,
        help="The rcodesign pkcs12 file."
    )
    rcodesign_group.add_argument(
        "-p", "--rcodesign-p12-password-file",
        default=None,
        help="The rcodesign pkcs12 password file."
    )
    return parser.parse_args()

def main():
    args = parse_args()
    logger = setup_logger(args.verbose)
    ctx = Context(logger)

    macos_sign(
        ctx,
        args.app_path,
        args.signing_identity,
        args.entitlements,
        args.channel,
        args.use_rcodesign,
        args.rcodesign_p12_file,
        args.rcodesign_p12_password_file,
        args.verbose
    )

def macos_sign(
    command_context,
    app_arg,
    signing_identity_arg,
    entitlements_arg,
    channel_arg,
    use_rcodesign_arg,
    p12_file_arg,
    p12_password_file_arg,
    verbose_arg,
):
    """Signs a .app build with entitlements from the repo

    Validates all the command line options, reads the signing config from
    the repo to determine which entitlement files to use, signs the build
    using either the native macOS codesign or rcodesign, and finally validates
    the .app using codesign.
    """
    command_context._set_log_level(verbose_arg)

    # Check if we're running on Linux, force rcodesign in that case
    current_platform = platform.system()
    if current_platform == "Linux":
        use_rcodesign_arg = True
        command_context.log(
            logging.INFO,
            "macos-sign",
            {"platform": current_platform},
            "Running on {platform}, forcing rcodesign usage",
        )

    # Check appdir and remove trailing slasshes
    if not os.path.isdir(app_arg):
        command_context.log(
            logging.ERROR,
            "macos-sign",
            {"app": app_arg},
            "ERROR: {app} is not a directory",
        )
        sys.exit(1)
    app = os.path.realpath(app_arg)

    # With rcodesign, either both a p12 file and p12 password file should be
    # provided or neither. If neither, the signing identity must be either '-'
    # for ad-hoc or None.
    rcodesign_p12_provided = False
    if use_rcodesign_arg:
        if p12_file_arg is None and p12_password_file_arg is not None:
            command_context.log(
                logging.ERROR,
                "macos-sign",
                {},
                "ERROR: p12 password file with no p12 file, " "use both or neither",
            )
            sys.exit(1)
        if p12_file_arg is not None and p12_password_file_arg is None:
            command_context.log(
                logging.ERROR,
                "macos-sign",
                {},
                "ERROR: p12 file with no p12 password file, " "use both or neither",
            )
            sys.exit(1)
        if p12_file_arg is not None and p12_password_file_arg is not None:
            rcodesign_p12_provided = True

    # Only rcodesign supports pkcs12 args
    if not use_rcodesign_arg and (
        p12_password_file_arg is not None or p12_file_arg is not None
    ):
        command_context.log(
            logging.ERROR,
            "macos-sign",
            {},
            "ERROR: pkcs12 signing not supported with "
            "native codesign, only rcodesign",
        )
        sys.exit(1)

    # Check the user didn't ask for a codesigning identity with rcodesign.
    # Check the user didn't ask for ad-hoc signing AND rcodesign pkcs12 signing.
    # Check the user didn't ask for ad-hoc signing with production entitlements.
    # Self-signing and ad-hoc signing are both incompatible with production
    # entitlements. (Library loading entitlements depend on the codesigning
    # team ID which is not set on self-signed/ad-hoc signatures and requires an
    # Apple-issued cert. Signing succeeds, but the bundle will not be
    # launchable.)
    if use_rcodesign_arg:
        # With rcodesign, only accept "-s -" or no -s argument.
        if not rcodesign_p12_provided:
            if signing_identity_arg is not None and signing_identity_arg != "-s":
                command_context.log(
                    logging.ERROR,
                    "macos-sign",
                    {},
                    "ERROR: rcodesign requires pkcs12 or " "ad-hoc signing",
                )
                sys.exit(1)

        # Did the user request a signing identity string and pkcs12 signing?
        if rcodesign_p12_provided and signing_identity_arg is not None:
            command_context.log(
                logging.ERROR,
                "macos-sign",
                {},
                "ERROR: both ad-hoc and pkcs12 signing " "requested",
            )
            sys.exit(1)

    # Is ad-hoc signing with production entitlements requested?
    if (
        (not rcodesign_p12_provided)
        and (signing_identity_arg is None or signing_identity_arg == "-")
        and (entitlements_arg != "developer")
    ):
        command_context.log(
            logging.ERROR,
            "macos-sign",
            {},
            "ERROR: " "Production entitlements and self-signing are " "not compatible",
        )
        sys.exit(1)

    # By default, use ad-hoc
    signing_identity = None
    signing_identity_label = None
    if use_rcodesign_arg and rcodesign_p12_provided:
        # signing_identity will not be used
        signing_identity_label = "pkcs12"
    elif signing_identity_arg is None or signing_identity_arg == "-":
        signing_identity = "-"
        signing_identity_label = "ad-hoc"
    else:
        signing_identity = signing_identity_arg
        signing_identity_label = signing_identity_arg

    command_context.log(
        logging.INFO,
        "macos-sign",
        {"id": signing_identity_label},
        "Using {id} signing identity",
    )

    # Which channel are we signing? Set 'channel' based on 'channel_arg'.
    channel = None
    if channel_arg == "auto":
        channel = auto_detect_channel(command_context, app)
    else:
        channel = channel_arg
    command_context.log(
        logging.INFO,
        "macos-sign",
        {"channel": channel},
        "Using {channel} channel signing configuration",
    )

    # Do we want production or developer entitlements? Set 'entitlements_key'
    # based on 'entitlements_arg'. In the buildconfig, developer entitlements
    # are labeled as "default".
    entitlements_key = None
    if entitlements_arg == "production":
        entitlements_key = "production"
    elif entitlements_arg == "developer":
        entitlements_key = "default"
    elif entitlements_arg == "production-without-restricted":
        # We'll strip out restricted entitlements below.
        entitlements_key = "production"

    command_context.log(
        logging.INFO,
        "macos-sign",
        {"ent": entitlements_arg},
        "Using {ent} entitlements",
    )

    # Get a path to the config file which maps files in the .app
    # bundle to their entitlement files in the tree (if any) to use
    # when signing. There is a set of mappings for each combination
    # of ({developer, production}, {nightly, devedition, release}).
    # i.e., depending on the entitlements argument (production or dev)
    # and the channel argument (nightly, devedition, or release) we'll
    # use different entitlements.
    sourcedir = os.getcwd()
    buildconfigpath = os.path.join(sourcedir, "taskcluster/config.yml")

    command_context.log(
        logging.INFO,
        "macos-sign",
        {"yaml": buildconfigpath},
        "Reading build config file {yaml}",
    )

    with open(buildconfigpath, "r") as buildconfigfile:
        parsedconfig = yaml.safe_load(buildconfigfile)

    # Store all the mappings
    signing_groups = parsedconfig["mac-signing"]["hardened-sign-config"][
        "by-hardened-signing-type"
    ][entitlements_key]

    command_context.log(
        logging.INFO, "macos-sign", {}, "Stripping existing xattrs and signatures"
    )

    # Use rcodesign for stripping xattrs and signatures if on Linux or if rcodesign is explicitly requested
    if use_rcodesign_arg or current_platform == "Linux":
        command_context.log(
            logging.INFO, "macos-sign", {}, "Using rcodesign to strip signatures"
        )
        # rcodesign has a command to remove signatures
        rcodesign_reset_cmd = ["rcodesign", "remove-signature", app, "--recursive"]
        run(command_context, rcodesign_reset_cmd, capture_output=not verbose_arg)
    else:
        # When on macOS, use the native tools or Python's built-in functionality
        try:
            # Try the xattr command first
            xattr_cmd = ["xattr", "-cr", app]
            run(command_context, xattr_cmd, capture_output=not verbose_arg)
        except Exception as e:
            command_context.log(
                logging.WARNING,
                "macos-sign",
                {"error": str(e)},
                "xattr command failed: {error}, falling back to Python method"
            )
            # Fall back to Python method to clear extended attributes
            clear_extended_attributes(command_context, app)

        # Remove existing signatures
        cs_reset_cmd = ["find", app, "-exec", "codesign", "--remove-signature", "{}", ";"]
        run(command_context, cs_reset_cmd, capture_output=not verbose_arg)

    if use_rcodesign_arg is True:
        sign_with_rcodesign(
            command_context,
            verbose_arg,
            signing_groups,
            entitlements_arg,
            channel,
            app,
            p12_file_arg,
            p12_password_file_arg,
        )
    else:
        sign_with_codesign(
            command_context,
            verbose_arg,
            signing_groups,
            signing_identity,
            entitlements_arg,
            channel,
            app,
        )

    # Modify verification to use rcodesign if running on Linux
    if current_platform == "Linux":
        verify_result_with_rcodesign(command_context, app, verbose_arg)
    else:
        verify_result(command_context, app, verbose_arg)


def auto_detect_channel(ctx, app):
    """Detects the channel of the provided app (nightly, release, etc.)

    Reads the CFBundleIdentifier from the provided apps Info.plist and
    returns the appropriate channel string. Release and Beta builds use
    org.mozilla.firefox for the CFBundleIdentifier. Nightly channel builds use
    org.mozilla.nightly.
    """
    # The bundle IDs for different channels. We use these strings to
    # auto-detect the channel being signed. Different channels use
    # different entitlement files.
    NIGHTLY_BUNDLEID = "org.mozilla.nightly"
    DEVEDITION_BUNDLEID = "org.mozilla.firefoxdeveloperedition"
    # BETA uses the same bundle ID as Release
    RELEASE_BUNDLEID = "org.mozilla.firefox"

    info_plist = os.path.join(app, "Contents/Info.plist")

    ctx.log(
        logging.DEBUG, "macos-sign", {"plist": info_plist}, "Reading {plist} bundle ID"
    )

    process = subprocess.Popen(
        ["defaults", "read", info_plist, "CFBundleIdentifier"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    out, err = process.communicate()
    bundleid = out.decode("utf-8").strip()

    ctx.log(
        logging.DEBUG,
        "macos-sign",
        {"bundleid": bundleid},
        "Found bundle ID {bundleid}",
    )

    if bundleid == NIGHTLY_BUNDLEID:
        return "nightly"
    elif bundleid == DEVEDITION_BUNDLEID:
        return "devedition"
    elif bundleid == RELEASE_BUNDLEID:
        return "release"
    else:
        # Couldn't determine the channel from <info_plist>.
        # Unrecognized bundle ID <bundleID>.
        # Use the channel argument.
        ctx.log(
            logging.ERROR,
            "macos-sign",
            {"plist": info_plist},
            "Couldn't read bundle ID from {plist}",
        )
        sys.exit(1)


def sign_with_codesign(
    ctx, verbose_arg, signing_groups, signing_identity, entitlements_arg, channel, app
):
    # Signing with codesign:
    #
    # For each signing_group in signing_groups, invoke codesign with the
    # options and paths specified in the signging_group.
    ctx.log(logging.INFO, "macos-sign", {}, "Signing with codesign")

    for signing_group in signing_groups:
        cs_cmd = ["codesign"]
        cs_cmd.append("--sign")
        cs_cmd.append(signing_identity)

        if "deep" in signing_group and signing_group["deep"]:
            cs_cmd.append("--deep")
        if "force" in signing_group and signing_group["force"]:
            cs_cmd.append("--force")
        if "runtime" in signing_group and signing_group["runtime"]:
            cs_cmd.append("--options")
            cs_cmd.append("runtime")

        entitlement_file = None
        temp_files_to_cleanup = []

        if "entitlements" in signing_group:
            # This signing group has an entitlement file
            cs_cmd.append("--entitlements")

            # Given the type of build (dev, prod, or prod without restricted
            # entitlements) and the channel we're going to sign, get the path
            # to the entitlement file from the config.
            if isinstance(signing_group["entitlements"], str):
                # If the 'entitlements' key in the signing group maps to
                # a string, it's a simple lookup.
                entitlement_file = signing_group["entitlements"]
            elif isinstance(signing_group["entitlements"], dict):
                # If the 'entitlements' key in the signing group maps to
                # a dict, the mapping from key to entitlement file is
                # different for each channel:
                if channel == "nightly":
                    entitlement_file = signing_group["entitlements"][
                        "by-build-platform"
                    ]["default"]["by-project"]["mozilla-central"]
                elif channel == "devedition":
                    entitlement_file = signing_group["entitlements"][
                        "by-build-platform"
                    ][".*devedition.*"]
                elif channel == "release" or channel == "beta":
                    entitlement_file = signing_group["entitlements"][
                        "by-build-platform"
                    ]["default"]["by-project"]["default"]
                else:
                    raise ("Unexpected channel")

            # We now have an entitlement file for this signing group.
            # If we are signing using production-without-restricted, strip out
            # restricted entitlements and save the result in a temporary file.
            if entitlements_arg == "production-without-restricted":
                temp_ent_file = strip_restricted_entitlements(entitlement_file)
                temp_files_to_cleanup.append(temp_ent_file)
                cs_cmd.append(temp_ent_file)
            else:
                cs_cmd.append(entitlement_file)

        for pathglob in signing_group["globs"]:
            binary_paths = glob.glob(
                os.path.join(app, pathglob.strip("/")), recursive=True
            )
            for binary_path in binary_paths:
                cs_cmd.append(binary_path)

        run(ctx, cs_cmd, capture_output=not verbose_arg, check=True)

        for temp_file in temp_files_to_cleanup:
            os.remove(temp_file)


def run(ctx, cmd, **kwargs):
    cmd_as_str = " ".join(cmd)
    ctx.log(logging.DEBUG, "macos-sign", {"cmd": cmd_as_str}, "[{cmd}]")
    try:
        # Check if the command exists before running it
        if shutil.which(cmd[0]) is None:
            raise FileNotFoundError(f"Command not found: {cmd[0]}")
        subprocess.run(cmd, **kwargs)
    except FileNotFoundError as e:
        ctx.log(
            logging.ERROR,
            "macos-sign",
            {"cmd": cmd[0], "error": str(e)},
            "Command {cmd} not found: {error}"
        )
        raise
    except subprocess.CalledProcessError as e:
        ctx.log(
            logging.ERROR,
            "macos-sign",
            {"rc": e.returncode, "cmd": cmd_as_str, "prog": cmd[0]},
            "{prog} subprocess failed with exit code {rc}. "
            "See (-v) verbose output for command output. "
            "Failing command: [{cmd}]",
        )
        sys.exit(e.returncode)


def verify_result(ctx, app, verbose_arg):
    # Verbosely verify validity of signed app
    cs_verify_cmd = ["codesign", "-vv", app]
    try:
        run(ctx, cs_verify_cmd, capture_output=not verbose_arg, check=True)
        ctx.log(
            logging.INFO,
            "macos-sign",
            {"app": app},
            "Verification of signed app {app} OK",
        )
    except subprocess.CalledProcessError as e:
        ctx.log(
            logging.ERROR,
            "macos-sign",
            {"rc": e.returncode, "app": app},
            "Verification of {app} failed with exit code {rc}",
        )
        sys.exit(e.returncode)


def sign_with_rcodesign(
    ctx,
    verbose_arg,
    signing_groups,
    entitlements_arg,
    channel,
    app,
    p12_file_arg,
    p12_password_file_arg,
):
    # Signing with rcodesign:
    #
    # The rcodesign sign is a single rcodesign invocation with all necessary
    # arguments included. rcodesign accepts signing options to be applied to
    # an input path (the .app in this case). For inner bundle resources that
    # have different codesigning settings, signing options are passed as
    # scoped arguments in the form --option <relative-path>:<value>. For
    # example, a different entitlement file is specified for the nested
    # plugin-container.app with the following:
    #
    # --entitlements-xml-path \
    #   Contents/MacOS/plugin-container.app:/path/to/plugin-container.xml
    #
    # We iterate through the signing group and generate scoped arguments
    # for each path to be signed. If the path is '/', it is the main signing
    # input path and its options are specified as standard arguments.
    ctx.log(logging.INFO, "macos-sign", {}, "Signing with rcodesign")

    cs_cmd = ["rcodesign", "sign"]
    if p12_file_arg is not None:
        cs_cmd.append("--p12-file")
        cs_cmd.append(p12_file_arg)
    if p12_password_file_arg is not None:
        cs_cmd.append("--p12-password-file")
        cs_cmd.append(p12_password_file_arg)

    temp_files_to_cleanup = []

    for signing_group in signing_groups:
        # Ignore the 'deep' and 'force' setting for rcodesign
        group_runtime = "runtime" in signing_group and signing_group["runtime"]

        entitlement_file = None

        if "entitlements" in signing_group:
            # Given the type of build (dev, prod, or prod without restricted
            # entitlements) and the channel we're going to sign, get the path
            # to the entitlement file from the config.
            if isinstance(signing_group["entitlements"], str):
                # If the 'entitlements' key in the signing group maps to
                # a string, it's a simple lookup.
                entitlement_file = signing_group["entitlements"]
            elif isinstance(signing_group["entitlements"], dict):
                # If the 'entitlements' key in the signing group maps to
                # a dict, the mapping from key to entitlement file is
                # different for each channel:
                if channel == "nightly":
                    entitlement_file = signing_group["entitlements"][
                        "by-build-platform"
                    ]["default"]["by-project"]["mozilla-central"]
                elif channel == "devedition":
                    entitlement_file = signing_group["entitlements"][
                        "by-build-platform"
                    ][".*devedition.*"]
                elif channel == "release" or channel == "beta":
                    entitlement_file = signing_group["entitlements"][
                        "by-build-platform"
                    ]["default"]["by-project"]["default"]
                else:
                    raise ("Unexpected channel")

            # We now have an entitlement file for this signing group.
            # If we are signing using production-without-restricted, strip out
            # restricted entitlements and save the result in a temporary file.
            if entitlements_arg == "production-without-restricted":
                entitlement_file = strip_restricted_entitlements(entitlement_file)
                temp_files_to_cleanup.append(entitlement_file)

        for pathglob in signing_group["globs"]:
            binary_paths = glob.glob(
                os.path.join(app, pathglob.strip("/")), recursive=True
            )
            for binary_path in binary_paths:
                if pathglob == "/":
                    # This is the root of the app. Use these signing options
                    # without argument scoping.
                    if group_runtime:
                        cs_cmd.append("--code-signature-flags")
                        cs_cmd.append("runtime")
                    if entitlement_file is not None:
                        cs_cmd.append("--entitlements-xml-path")
                        cs_cmd.append(entitlement_file)
                    cs_cmd.append(binary_path)
                    continue

                # This is not the root of the app. Paths are convered to
                # relative paths and signing options are specified as scoped
                # arguments.
                binary_path_relative = os.path.relpath(binary_path, app)
                if group_runtime:
                    cs_cmd.append("--code-signature-flags")
                    scoped_arg = binary_path_relative + ":runtime"
                    cs_cmd.append(scoped_arg)
                if entitlement_file is not None:
                    cs_cmd.append("--entitlements-xml-path")
                    scoped_arg = binary_path_relative + ":" + entitlement_file
                    cs_cmd.append(scoped_arg)

    run(ctx, cs_cmd, capture_output=not verbose_arg, check=True)

    for temp_file in temp_files_to_cleanup:
        os.remove(temp_file)


def strip_restricted_entitlements(plist_file):
    # Not a complete set. Update as needed. This is
    # the set of restricted entitlements we use to date.
    restricted_entitlements = [
        "com.apple.developer.web-browser.public-key-credential",
        "com.apple.application-identifier",
    ]

    plist_file_obj = open(plist_file, "rb")
    plist_data = plistlib.load(plist_file_obj, fmt=plistlib.FMT_XML)
    for entitlement in restricted_entitlements:
        if entitlement in plist_data:
            del plist_data[entitlement]

    _, temp_file_path = tempfile.mkstemp(prefix="mach-macos-sign.")
    with open(temp_file_path, "wb") as temp_file_obj:
        plistlib.dump(plist_data, temp_file_obj)
        temp_file_obj.close()

    return temp_file_path


def verify_result_with_rcodesign(ctx, app, verbose_arg):
    """
    Verify signature validity using rcodesign.
    Unlike codesign, rcodesign verify needs to be run on specific Mach-O binaries,
    not on the entire .app bundle.
    """
    ctx.log(
        logging.INFO,
        "macos-sign",
        {"app": app},
        "Verifying signatures with rcodesign in {app}"
    )
    
    # Find the main executable in the bundle
    app_name = os.path.basename(app)
    if app_name.endswith('.app'):
        app_name = app_name[:-4]
    
    # Main executable path is usually at Contents/MacOS/AppName
    main_executable = os.path.join(app, "Contents/MacOS", app_name)
    
    # If the main executable doesn't exist with that name, find the first executable in MacOS directory
    if not os.path.exists(main_executable):
        macos_dir = os.path.join(app, "Contents/MacOS")
        if os.path.isdir(macos_dir):
            executables = [f for f in os.listdir(macos_dir) if os.path.isfile(os.path.join(macos_dir, f))]
            if executables:
                main_executable = os.path.join(macos_dir, executables[0])
        
    if not os.path.exists(main_executable):
        ctx.log(
            logging.ERROR,
            "macos-sign",
            {"app": app},
            "Could not find main executable in {app}",
        )
        sys.exit(1)
    
    # Verify the main executable
    rcs_verify_cmd = ["rcodesign", "verify", main_executable, "--verbose"]
    try:
        run(ctx, rcs_verify_cmd, capture_output=not verbose_arg, check=True)
        ctx.log(
            logging.INFO,
            "macos-sign",
            {"bin": main_executable},
            "Verification of main binary {bin} with rcodesign OK",
        )
    except subprocess.CalledProcessError as e:
        ctx.log(
            logging.ERROR,
            "macos-sign",
            {"rc": e.returncode, "bin": main_executable},
            "Verification of {bin} with rcodesign failed with exit code {rc}",
        )
        sys.exit(e.returncode)

    # Additionally, verify other important binaries in the bundle
    frameworks_dir = os.path.join(app, "Contents/Frameworks")
    if os.path.isdir(frameworks_dir):
        ctx.log(
            logging.INFO,
            "macos-sign",
            {"dir": frameworks_dir},
            "Checking signatures of frameworks in {dir}",
        )
        # Sample key framework to check - adjust as needed for Firefox
        for framework in ["XUL.framework"]:
            framework_path = os.path.join(frameworks_dir, framework)
            if os.path.exists(framework_path):
                # Find the main binary in the framework
                versions_dir = os.path.join(framework_path, "Versions")
                if os.path.isdir(versions_dir):
                    for version in os.listdir(versions_dir):
                        if version != "Current":
                            bin_path = os.path.join(versions_dir, version, framework[:-10])  # Remove .framework
                            if os.path.exists(bin_path) and os.path.isfile(bin_path):
                                try:
                                    rcs_verify_cmd = ["rcodesign", "verify", bin_path, "--verbose"]
                                    run(ctx, rcs_verify_cmd, capture_output=not verbose_arg, check=True)
                                    ctx.log(
                                        logging.INFO,
                                        "macos-sign",
                                        {"bin": bin_path},
                                        "Verification of framework binary {bin} OK",
                                    )
                                except subprocess.CalledProcessError as e:
                                    ctx.log(
                                        logging.WARNING,
                                        "macos-sign",
                                        {"rc": e.returncode, "bin": bin_path},
                                        "Verification of {bin} failed with exit code {rc}",
                                    )
                                    # Continue checking other binaries instead of exiting

    ctx.log(
        logging.INFO,
        "macos-sign",
        {"app": app},
        "Verification of app bundle {app} completed",
    )


def clear_extended_attributes(ctx, path):
    """
    Clear extended attributes using Python's built-in functions.
    This is a cross-platform alternative to the xattr command.
    """
    ctx.log(logging.INFO, "macos-sign", {"path": path}, "Clearing extended attributes from {path}")

    def _clear_attrs_for_file(file_path):
        try:
            # On macOS
            if hasattr(os, "listxattr"):
                attrs = os.listxattr(file_path, follow_symlinks=False)
                for attr in attrs:
                    try:
                        os.removexattr(file_path, attr, follow_symlinks=False)
                    except Exception as e:
                        ctx.log(logging.DEBUG, "macos-sign", 
                               {"file": file_path, "attr": attr, "error": str(e)}, 
                               "Failed to remove xattr {attr} from {file}: {error}")
            # No xattr support - just continue
        except Exception as e:
            ctx.log(logging.DEBUG, "macos-sign", 
                   {"file": file_path, "error": str(e)}, 
                   "Failed to process xattrs for {file}: {error}")

    # Process the main path
    if os.path.isfile(path):
        _clear_attrs_for_file(path)

    # Walk the directory structure
    for root, dirs, files in os.walk(path):
        for file in files:
            _clear_attrs_for_file(os.path.join(root, file))
        for dir in dirs:
            _clear_attrs_for_file(os.path.join(root, dir))

if __name__ == "__main__":
    main()
