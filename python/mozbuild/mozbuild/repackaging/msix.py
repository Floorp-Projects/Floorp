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

    return 0
