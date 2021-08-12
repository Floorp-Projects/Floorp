# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

r"""Repackage ZIP archives (or directories) into MSIX App Packages.

# Known issues

- The icons in the Start Menu have a solid colour tile behind them.  I think
  this is an issue with plating.
"""

from __future__ import absolute_import, print_function

import logging
import os
import sys
import subprocess
import time
import urllib

from six.moves import shlex_quote

from mozboot.util import get_state_dir
from mozbuild.util import ensureParentDir
from mozfile import which
from mozpack.copier import FileCopier
from mozpack.files import FileFinder, JarFinder
from mozpack.manifests import InstallManifest
from mozpack.mozjar import JarReader
from mozpack.packager.unpack import UnpackFinder
from mozbuild.repackaging.application_ini import get_application_ini_values
import mozpack.path as mozpath


def log_copy_result(log, elapsed, destdir, result):
    COMPLETE = (
        "Elapsed: {elapsed:.2f}s; From {dest}: Kept {existing} existing; "
        "Added/updated {updated}; "
        "Removed {rm_files} files and {rm_dirs} directories."
    )
    copy_result = COMPLETE.format(
        elapsed=elapsed,
        dest=destdir,
        existing=result.existing_files_count,
        updated=result.updated_files_count,
        rm_files=result.removed_files_count,
        rm_dirs=result.removed_directories_count,
    )
    log(logging.INFO, "msix", {"copy_result": copy_result}, "{copy_result}")


# See https://docs.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-identity.
_MSIX_ARCH = {"x86": "x86", "x86_64": "x64", "aarch64": "arm64"}


def find_sdk_tool(binary, log=None):
    if binary.lower().endswith(".exe"):
        binary = binary[:-4]

    maybe = os.environ.get(binary.upper())
    if maybe:
        log(
            logging.DEBUG,
            "msix",
            {"binary": binary, "path": maybe},
            "Found {binary} in environment: {path}",
        )
        return mozpath.normsep(maybe)

    maybe = which(
        binary, extra_search_dirs=["c:/Windows/System32/WindowsPowershell/v1.0"]
    )
    if maybe:
        log(
            logging.DEBUG,
            "msix",
            {"binary": binary, "path": maybe},
            "Found {binary} on path: {path}",
        )
        return mozpath.normsep(maybe)

    sdk = os.environ.get("WINDOWSSDKDIR") or "C:/Program Files (x86)/Windows Kits/10"
    log(
        logging.DEBUG,
        "msix",
        {"binary": binary, "sdk": sdk},
        "Looking for {binary} in Windows SDK: {sdk}",
    )

    if sdk:
        # Like `bin/VERSION/ARCH/tool.exe`.
        finder = FileFinder(sdk)

        # TODO: handle running on ARM.
        is_64bits = sys.maxsize > 2 ** 32
        arch = "x64" if is_64bits else "x86"

        for p, f in finder.find(
            "bin/**/{arch}/{binary}.exe".format(arch=arch, binary=binary)
        ):
            maybe = mozpath.normsep(mozpath.join(sdk, p))
            log(
                logging.DEBUG,
                "msix",
                {"binary": binary, "path": maybe},
                "Found {binary} in Windows SDK: {path}",
            )
            return maybe

    return None


def get_embedded_version(version, buildid):
    r"""Turn a display version into "dotted quad" notation."""

    # It's irritating to roll our own version parsing, but the tree doesn't seem
    # to contain exactly what we need at this time.
    version = version.rsplit("esr", 1)[0]
    alpha = "a" in version

    if "a" in version:
        head, tail = version.rsplit("a", 1)
        tail = buildid
    elif "b" in version:
        head, tail = version.rsplit("b", 1)
    elif "rc" in version:
        head, tail = version.rsplit("rc", 1)
    else:
        head = version
        tail = "0"

    components = (head.split(".") + ["0", "0", "0"])[:4]
    components[3] = tail

    if alpha:
        # Nightly builds are all `X.Ya1`, which isn't helpful.  Include build ID
        # to disambiguate.  But each part of the dotted quad is 16 bits, so we
        # have to squash.
        year = buildid[0:4]
        month = buildid[4:6]
        if month[0] == "0":
            month = month[1]
        day = buildid[6:8]
        hour = buildid[8:10]
        if hour[0] == "0":
            hour = hour[1]
        minute = buildid[10:12]

        components[1] = year
        components[2] = "".join((month, day))
        components[3] = "".join((hour, minute))

    version = "{}.{}.{}.{}".format(*components)

    return version


def repackage_msix(
    dir_or_package,
    channel=None,
    branding=None,
    template=None,
    distribution_dirs=[],
    version=None,
    vendor="Mozilla",
    displayname=None,
    app_name="firefox",
    identity=None,
    arch=None,
    publisher=None,
    output=None,
    force=False,
    log=None,
    verbose=False,
    makeappx=None,
):
    if not channel:
        raise Exception("channel is required")
    if channel not in ["official", "beta", "aurora", "nightly", "unofficial"]:
        raise Exception("channel is unrecognized: {}".format(channel))

    if not branding:
        raise Exception("branding dir is required")
    if not os.path.isdir(branding):
        raise Exception("branding dir {} does not exist".format(branding))

        version = "1.2.3"  # XXX
    # TODO: maybe we can fish this from the package directly?  Maybe from a DLL,
    # maybe from application.ini?
    if arch is None or arch not in _MSIX_ARCH.keys():
        raise Exception(
            "arch name must be provided and one of {}.".format(_MSIX_ARCH.keys())
        )

    if not os.path.exists(dir_or_package):
        raise Exception("{} does not exist".format(dir_or_package))

    if os.path.isdir(dir_or_package):
        finder = FileFinder(dir_or_package)
    else:
        finder = JarFinder(dir_or_package, JarReader(dir_or_package))

    values = get_application_ini_values(
        finder,
        dict(section="App", value="CodeName", fallback="Name"),
        dict(section="App", value="Version"),
        dict(section="App", value="BuildID"),
    )
    first = next(values)
    displayname = displayname or first
    if not version:
        version = next(values)
        buildid = next(values)
        version = get_embedded_version(version, buildid)

    # We don't have a build at repackage-time to gives us this value, and the
    # source of truth is a branding-specific `configure.sh` shell script that we
    # can't easily evaluate completely here.  Instead, we take the last value
    # from `configure.sh`.
    lines = [
        line
        for line in open(mozpath.join(branding, "configure.sh")).readlines()
        if "MOZ_IGECKOBACKCHANNEL_IID" in line
    ]
    MOZ_IGECKOBACKCHANNEL_IID = lines[-1]
    _, _, MOZ_IGECKOBACKCHANNEL_IID = MOZ_IGECKOBACKCHANNEL_IID.partition("=")
    MOZ_IGECKOBACKCHANNEL_IID = MOZ_IGECKOBACKCHANNEL_IID.strip()
    if MOZ_IGECKOBACKCHANNEL_IID.startswith(('"', "'")):
        MOZ_IGECKOBACKCHANNEL_IID = MOZ_IGECKOBACKCHANNEL_IID[1:-1]

    # TODO: Bug 1721922: localize this description via Fluent.
    # For `brand.properties`, which is in the omnijar in packaged builds.
    unpack_finder = UnpackFinder(finder)
    lines = []
    for _, f in unpack_finder.find("**/chrome/en-US/locale/branding/brand.properties"):
        lines.extend(
            line
            for line in f.open().read().decode("utf-8").splitlines()
            if "brandFullName" in line
        )
    (brandFullName,) = lines  # We expect exactly one definition.
    _, _, brandFullName = brandFullName.partition("=")
    brandFullName = brandFullName.strip()

    # The convention is $MOZBUILD_STATE_PATH/cache/$FEATURE.
    output_dir = mozpath.normsep(
        mozpath.join(
            get_state_dir(), "cache", "mach-msix", "msix-temp-{}".format(channel)
        )
    )

    if channel == "beta":
        # Release (official) and Beta share branding.  Differentiate Beta a little bit.
        displayname += " Beta"
        brandFullName += " Beta"

    # Like 'Firefox Package Root', 'Firefox Nightly Package Root', 'Firefox Beta
    # Package Root'.  This is `BrandFullName` in the installer, and we want to
    # be close but to not match.  By not matching, we hope to prevent confusion
    # and/or errors between regularly installed builds and App Package builds.
    instdir = "{} Package Root".format(displayname)

    # Microsoft names its packages like "Microsoft.VCLibs.140.00.UWPDesktop".
    # Hyphenated (kebab) names seem to install correctly, but `Remove-AppPackage
    # kebab-name` seems to fail in some cases.
    identity = identity or "{}.{}".format(vendor, displayname.replace(" ", "."))

    defines = {
        "APPX_ARCH": _MSIX_ARCH[arch],
        "APPX_DISPLAYNAME": brandFullName,
        "APPX_DESCRIPTION": brandFullName,
        # Like 'Mozilla.Firefox', 'Mozilla.Firefox.Beta', 'Mozilla.Firefox.Nightly'.
        "APPX_IDENTITY": identity,
        # Like 'Firefox Package Root', 'Firefox Nightly Package Root', 'Firefox
        # Beta Package Root'.  See above.
        "APPX_INSTDIR": instdir,
        # Like 'Firefox%20Package%20Root'.
        "APPX_INSTDIR_QUOTED": urllib.parse.quote(instdir),
        "APPX_PUBLISHER": publisher,
        "APPX_VERSION": version,
        "MOZ_APP_DISPLAYNAME": displayname,
        "MOZ_APP_NAME": app_name,
        "MOZ_APP_VENDOR": vendor,
        "MOZ_IGECKOBACKCHANNEL_IID": MOZ_IGECKOBACKCHANNEL_IID,
    }

    # We might want to include the publisher ID hash here.  I.e.,
    # "__{publisherID}".  My locally produced MSIX was named like
    # `Mozilla.Firefox.Nightly_89.0.0.0_x64__4gf61r4q480j0`, suggesting also a
    # missing field, but it's necessary, since this is just an output file name.
    package_output_name = "{identity}_{version}_{arch}".format(
        identity=identity, version=version, arch=_MSIX_ARCH[arch]
    )
    # The convention is $MOZBUILD_STATE_PATH/cache/$FEATURE.
    default_output = mozpath.normsep(
        mozpath.join(
            get_state_dir(), "cache", "mach-msix", "{}.msix".format(package_output_name)
        )
    )
    output = output or default_output
    log(logging.INFO, "msix", {"output": output}, "Repackaging to: {output}")

    m = InstallManifest()
    m.add_preprocess(
        mozpath.join(template, "AppxManifest.xml.in"),
        "AppxManifest.xml",
        [],
        defines=defines,
        marker="<!-- #",  # So that we can have well-formed XML.
    )
    m.add_copy(mozpath.join(template, "Resources.pri"), "Resources.pri")

    m.add_pattern_copy(mozpath.join(branding, "msix", "Assets"), "**", "Assets")
    m.add_pattern_copy(mozpath.join(template, "VFS"), "**", "VFS")

    copier = FileCopier()
    m.populate_registry(copier)

    # TODO: Bug 1710147: filter out MSVCRT files and use a dependency instead.
    for p, f in finder:
        # `p` is like "firefox/firefox.exe"; we want just "firefox.exe".
        pp = os.path.relpath(p, "firefox")
        copier.add(mozpath.normsep(mozpath.join("VFS", "ProgramFiles", instdir, pp)), f)

    for distribution_dir in [
        mozpath.join(template, "distribution")
    ] + distribution_dirs:
        log(
            logging.INFO,
            "msix",
            {"dir": distribution_dir},
            "Adding distribution files from {dir}",
        )

        # In automation, we have no easy way to remap the names of artifacts fetched from dependent
        # tasks.  In particular, langpacks will be named like `target.langpack.xpi`.  The fetch
        # tasks do allow us to put them in a per-locale directory, so that the entire set can be
        # fetched.  Here we remap the names.
        finder = FileFinder(distribution_dir)

        for p, f in finder:
            if os.path.basename(p) == "target.langpack.xpi":
                # Turn "/path/to/LOCALE/target.langpack.xpi" into "LOCALE".
                base, locale = os.path.split(os.path.dirname(p))

                # Like "langpack-LOCALE@firefox.mozilla.org.xpi".  This is what AMO serves and how
                # flatpak builds name langpacks, but not how snap builds name langpacks.  I can't
                # explain the discrepancy.
                dest = mozpath.normsep(
                    mozpath.join(base, f"langpack-{locale}@firefox.mozilla.org.xpi")
                )

                log(
                    logging.DEBUG,
                    "msix",
                    {"path": p, "dest": dest},
                    "Renaming langpack {path} to {dest}",
                )
            else:
                dest = p

            copier.add(
                mozpath.normsep(
                    mozpath.join("VFS", "ProgramFiles", instdir, "distribution", dest)
                ),
                f,
            )

    output_dir = mozpath.abspath(output_dir)
    ensureParentDir(output_dir)

    start = time.time()
    result = copier.copy(
        output_dir, remove_empty_directories=True, skip_if_older=not force
    )
    if log:
        log_copy_result(log, time.time() - start, output_dir, result)

    if not makeappx:
        makeappx = find_sdk_tool("makeappx.exe", log=log)
    if not makeappx:
        raise ValueError(
            "makeappx is required; " "set SIGNTOOL or WINDOWSSDKDIR or PATH"
        )

    # `makeappx.exe` supports both slash and hyphen style arguments; `makemsix`
    # supports only hyphen style.  `makeappx.exe` allows to overwrite and to
    # provide more feedback, so we prefer invoking with these flags.  This will
    # also accommodate `wine makeappx.exe`.
    stdout = subprocess.run(
        [makeappx], check=False, capture_output=True, universal_newlines=True
    ).stdout
    is_makeappx = "MakeAppx Tool" in stdout

    if is_makeappx:
        args = [makeappx, "pack", "/d", output_dir, "/p", output, "/overwrite"]
    else:
        args = [makeappx, "pack", "-d", output_dir, "-p", output]
    if verbose and is_makeappx:
        args.append("/verbose")
    joined = " ".join(shlex_quote(arg) for arg in args)
    log(logging.INFO, "msix", {"args": args, "joined": joined}, "Invoking: {joined}")

    sys.stdout.flush()  # Otherwise the subprocess output can be interleaved.
    if verbose:
        subprocess.check_call(args, universal_newlines=True)
    else:
        # Suppress output unless we fail.
        try:
            subprocess.check_output(args, universal_newlines=True)
        except subprocess.CalledProcessError as e:
            sys.stderr.write(e.output)
            raise

    return output


def sign_msix(output, force=False, log=None, verbose=False):
    """Sign an MSIX with a locally generated self-signed certificate."""

    # TODO: sign on non-Windows hosts.
    if sys.platform != "win32":
        raise Exception("sign msix only works on Windows")

    powershell_exe = find_sdk_tool("powershell.exe", log=log)
    if not powershell_exe:
        raise ValueError("powershell is required; " "set POWERSHELL or PATH")

    def powershell(argstring, check=True):
        "Invoke `powershell.exe`.  Arguments are given as a string to allow consumer to quote."
        args = [powershell_exe, "-c", argstring]
        joined = " ".join(shlex_quote(arg) for arg in args)
        log(
            logging.INFO, "msix", {"args": args, "joined": joined}, "Invoking: {joined}"
        )
        return subprocess.run(
            args, check=check, universal_newlines=True, capture_output=True
        ).stdout

    signtool = find_sdk_tool("signtool.exe", log=log)
    if not signtool:
        raise ValueError(
            "signtool is required; " "set SIGNTOOL or WINDOWSSDKDIR or PATH"
        )

    # Our first order of business is to find, or generate, a (self-signed)
    # certificate.

    # These are baked into enough places under `browser/` that we need not
    # extract constants.
    vendor = "Mozilla"
    publisher = "CN=Mozilla Corporation, OU=MSIX Packaging"
    friendly_name = "Mozilla Corporation MSIX Packaging Test Certificate"

    # The convention is $MOZBUILD_STATE_PATH/cache/$FEATURE.
    crt_path = mozpath.join(
        get_state_dir(),
        "cache",
        "mach-msix",
        "{}.crt".format(friendly_name).replace(" ", "_").lower(),
    )
    crt_path = mozpath.abspath(crt_path)
    ensureParentDir(crt_path)

    pfx_path = crt_path.replace(".crt", ".pfx")

    # TODO: maybe use an actual password.  For now, just something that won't be
    # brute-forced.
    password = "193dbfc6-8ff7-4a95-8f32-6b4468626bd0"

    if force or not os.path.isfile(crt_path):
        log(
            logging.INFO,
            "msix",
            {"crt_path": crt_path},
            "Creating new self signed certificate at: {}".format(crt_path),
        )

        thumbprints = [
            thumbprint.strip()
            for thumbprint in powershell(
                (
                    "Get-ChildItem -Path Cert:\CurrentUser\My"
                    '| Where-Object {{$_.Subject -Match "{}"}}'
                    '| Where-Object {{$_.FriendlyName -Match "{}"}}'
                    "| Select-Object -ExpandProperty Thumbprint"
                ).format(vendor, friendly_name)
            ).splitlines()
        ]
        if len(thumbprints) > 1:
            raise Exception(
                "Multiple certificates with friendly name found: {}".format(
                    friendly_name
                )
            )

        if len(thumbprints) == 1:
            thumbprint = thumbprints[0]
        else:
            thumbprint = None

        if not thumbprint:
            thumbprint = (
                powershell(
                    (
                        'New-SelfSignedCertificate -Type Custom -Subject "{}" '
                        '-KeyUsage DigitalSignature -FriendlyName "{}"'
                        " -CertStoreLocation Cert:\CurrentUser\My"
                        ' -TextExtension @("2.5.29.37={{text}}1.3.6.1.5.5.7.3.3", '
                        '"2.5.29.19={{text}}")'
                        "| Select-Object -ExpandProperty Thumbprint"
                    ).format(publisher, friendly_name)
                )
                .strip()
                .upper()
            )

        if not thumbprint:
            raise Exception(
                "Failed to find or create certificate with friendly name: {}".format(
                    friendly_name
                )
            )

        powershell(
            'Export-Certificate -Cert Cert:\CurrentUser\My\{} -FilePath "{}"'.format(
                thumbprint, crt_path
            )
        )
        log(
            logging.INFO,
            "msix",
            {"crt_path": crt_path},
            "Exported public certificate: {crt_path}",
        )

        powershell(
            (
                'Export-PfxCertificate -Cert Cert:\CurrentUser\My\{} -FilePath "{}"'
                ' -Password (ConvertTo-SecureString -String "{}" -Force -AsPlainText)'
            ).format(thumbprint, pfx_path, password)
        )
        log(
            logging.INFO,
            "msix",
            {"pfx_path": pfx_path},
            "Exported private certificate: {pfx_path}",
        )

    # Second, to find the right thumbprint to use.  We do this here in case
    # we're coming back to an existing certificate.

    log(
        logging.INFO,
        "msix",
        {"crt_path": crt_path},
        "Signing with existing self signed certificate: {crt_path}",
    )

    thumbprints = [
        thumbprint.strip()
        for thumbprint in powershell(
            'Get-PfxCertificate -FilePath "{}" | Select-Object -ExpandProperty Thumbprint'.format(
                crt_path
            )
        ).splitlines()
    ]
    if len(thumbprints) > 1:
        raise Exception("Multiple thumbprints found for PFX: {}".format(pfx_path))
    if len(thumbprints) == 0:
        raise Exception("No thumbprints found for PFX: {}".format(pfx_path))
    thumbprint = thumbprints[0]
    log(
        logging.INFO,
        "msix",
        {"thumbprint": thumbprint},
        "Signing with certificate with thumbprint: {thumbprint}",
    )

    # Third, do the actual signing.

    args = [
        signtool,
        "sign",
        "/a",
        "/fd",
        "SHA256",
        "/f",
        pfx_path,
        "/p",
        password,
        output,
    ]
    if not verbose:
        subprocess.check_call(args, universal_newlines=True)
    else:
        # Suppress output unless we fail.
        try:
            subprocess.check_output(args, universal_newlines=True)
        except subprocess.CalledProcessError as e:
            sys.stderr.write(e.output)
            raise

    # As a convenience to the user, tell how to use this certificate if it's not
    # already trusted, and how to work with MSIX files more generally.
    if verbose:
        root_thumbprints = [
            root_thumbprint.strip()
            for root_thumbprint in powershell(
                "Get-ChildItem -Path Cert:\LocalMachine\Root\{} "
                "| Select-Object -ExpandProperty Thumbprint".format(thumbprint),
                check=False,
            ).splitlines()
        ]
        if thumbprint not in root_thumbprints:
            log(
                logging.INFO,
                "msix",
                {"thumbprint": thumbprint},
                "Certificate with thumbprint not found in trusted roots: {thumbprint}",
            )
            log(
                logging.INFO,
                "msix",
                {"crt_path": crt_path, "output": output},
                r"""\
# Usage
To trust this certificate (requires an elevated shell):
powershell -c 'Import-Certificate -FilePath "{crt_path}" -Cert Cert:\LocalMachine\Root\'
To verify this MSIX signature exists and is trusted:
powershell -c 'Get-AuthenticodeSignature -FilePath "{output}" | Format-List *'
To install this MSIX:
powershell -c 'Add-AppPackage -path "{output}"'
To see details after installing:
powershell -c 'Get-AppPackage -name Mozilla.Firefox(.Beta,...)'
                """.strip(),
            )

    return 0
