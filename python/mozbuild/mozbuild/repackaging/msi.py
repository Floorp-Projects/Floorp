# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
import sys
import tempfile
from xml.dom import minidom

import mozpack.path as mozpath

from mozbuild.util import ensureParentDir

_MSI_ARCH = {
    "x86": "x86",
    "x86_64": "x64",
}


def update_wsx(wfile, pvalues):
    parsed = minidom.parse(wfile)

    # construct a dictinary for the pre-processing options
    # iterate over that list and add them to the wsx xml doc
    for k, v in pvalues.items():
        entry = parsed.createProcessingInstruction("define", k + ' = "' + v + '"')
        root = parsed.firstChild
        parsed.insertBefore(entry, root)
    # write out xml to new wfile
    new_w_file = wfile + ".new"
    with open(new_w_file, "w") as fh:
        parsed.writexml(fh)
    shutil.move(new_w_file, wfile)
    return wfile


def repackage_msi(
    topsrcdir, wsx, version, locale, arch, setupexe, candle, light, output
):
    if sys.platform != "win32":
        raise Exception("repackage msi only works on windows")
    if not os.path.isdir(topsrcdir):
        raise Exception("%s does not exist." % topsrcdir)
    if not os.path.isfile(wsx):
        raise Exception("%s does not exist." % wsx)
    if version is None:
        raise Exception("version name must be provided.")
    if locale is None:
        raise Exception("locale name must be provided.")
    if arch is None or arch not in _MSI_ARCH.keys():
        raise Exception(
            "arch name must be provided and one of {}.".format(_MSI_ARCH.keys())
        )
    if not os.path.isfile(setupexe):
        raise Exception("%s does not exist." % setupexe)
    if candle is not None and not os.path.isfile(candle):
        raise Exception("%s does not exist." % candle)
    if light is not None and not os.path.isfile(light):
        raise Exception("%s does not exist." % light)
    embeddedVersion = "0.0.0.0"
    # Version string cannot contain 'a' or 'b' when embedding in msi manifest.
    if "a" not in version and "b" not in version:
        if version.endswith("esr"):
            parts = version[:-3].split(".")
        else:
            parts = version.split(".")
        while len(parts) < 4:
            parts.append("0")
        embeddedVersion = ".".join(parts)

    wsx = mozpath.realpath(wsx)
    setupexe = mozpath.realpath(setupexe)
    output = mozpath.realpath(output)
    ensureParentDir(output)

    if sys.platform == "win32":
        tmpdir = tempfile.mkdtemp()
        old_cwd = os.getcwd()
        try:
            wsx_file = os.path.split(wsx)[1]
            shutil.copy(wsx, tmpdir)
            temp_wsx_file = os.path.join(tmpdir, wsx_file)
            temp_wsx_file = mozpath.realpath(temp_wsx_file)
            pre_values = {
                "Vendor": "Mozilla",
                "BrandFullName": "Mozilla Firefox",
                "Version": version,
                "AB_CD": locale,
                "Architecture": _MSI_ARCH[arch],
                "ExeSourcePath": setupexe,
                "EmbeddedVersionCode": embeddedVersion,
            }
            # update wsx file with inputs from
            newfile = update_wsx(temp_wsx_file, pre_values)
            wix_object_file = os.path.join(tmpdir, "installer.wixobj")
            env = os.environ.copy()
            if candle is None:
                candle = "candle.exe"
            cmd = [candle, "-out", wix_object_file, newfile]
            subprocess.check_call(cmd, env=env)
            wix_installer = wix_object_file.replace(".wixobj", ".msi")
            if light is None:
                light = "light.exe"
            light_cmd = [
                light,
                "-cultures:neutral",
                "-sw1076",
                "-sw1079",
                "-out",
                wix_installer,
                wix_object_file,
            ]
            subprocess.check_call(light_cmd, env=env)
            os.remove(wix_object_file)
            # mv file to output dir
            shutil.move(wix_installer, output)
        finally:
            os.chdir(old_cwd)
            shutil.rmtree(tmpdir)
