# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform
import shutil
import subprocess
from pathlib import Path
from typing import List

import mozfile

from mozbuild.util import ensureParentDir

is_linux = platform.system() == "Linux"
is_osx = platform.system() == "Darwin"


def chmod(dir):
    "Set permissions of DMG contents correctly"
    subprocess.check_call(["chmod", "-R", "a+rX,a-st,u+w,go-w", dir])


def rsync(source: Path, dest: Path):
    "rsync the contents of directory source into directory dest"
    # Ensure a trailing slash on directories so rsync copies the *contents* of source.
    raw_source = str(source)
    if source.is_dir():
        raw_source = str(source) + "/"
    subprocess.check_call(["rsync", "-a", "--copy-unsafe-links", raw_source, dest])


def set_folder_icon(dir: Path, tmpdir: Path, hfs_tool: Path = None):
    "Set HFS attributes of dir to use a custom icon"
    if is_linux:
        hfs = tmpdir / "staged.hfs"
        subprocess.check_call([hfs_tool, hfs, "attr", "/", "C"])
    elif is_osx:
        subprocess.check_call(["SetFile", "-a", "C", dir])


def generate_hfs_file(
    stagedir: Path, tmpdir: Path, volume_name: str, mkfshfs_tool: Path
):
    """
    When cross compiling, we zero fill an hfs file, that we will turn into
    a DMG. To do so we test the size of the staged dir, and add some slight
    padding to that.
    """
    hfs = tmpdir / "staged.hfs"
    output = subprocess.check_output(["du", "-s", stagedir])
    size = int(output.split()[0]) / 1000  # Get in MB
    size = int(size * 1.02)  # Bump the used size slightly larger.
    # Setup a proper file sized out with zero's
    subprocess.check_call(
        [
            "dd",
            "if=/dev/zero",
            "of={}".format(hfs),
            "bs=1M",
            "count={}".format(size),
        ]
    )
    subprocess.check_call([mkfshfs_tool, "-v", volume_name, hfs])


def create_app_symlink(stagedir: Path, tmpdir: Path, hfs_tool: Path = None):
    """
    Make a symlink to /Applications. The symlink name is a space
    so we don't have to localize it. The Applications folder icon
    will be shown in Finder, which should be clear enough for users.
    """
    if is_linux:
        hfs = os.path.join(tmpdir, "staged.hfs")
        subprocess.check_call([hfs_tool, hfs, "symlink", "/ ", "/Applications"])
    elif is_osx:
        os.symlink("/Applications", stagedir / " ")


def create_dmg_from_staged(
    stagedir: Path,
    output_dmg: Path,
    tmpdir: Path,
    volume_name: str,
    hfs_tool: Path = None,
    dmg_tool: Path = None,
    mkfshfs_tool: Path = None,
    attribution_sentinel: str = None,
):
    "Given a prepared directory stagedir, produce a DMG at output_dmg."

    if is_linux:
        # The dmg tool doesn't create the destination directories, and silently
        # returns success if the parent directory doesn't exist.
        ensureParentDir(output_dmg)
        hfs = os.path.join(tmpdir, "staged.hfs")
        subprocess.check_call([hfs_tool, hfs, "addall", stagedir])

        dmg_cmd = [dmg_tool, "build", hfs, output_dmg]
        if attribution_sentinel:
            while len(attribution_sentinel) < 1024:
                attribution_sentinel += "\t"
            subprocess.check_call(
                [
                    hfs_tool,
                    hfs,
                    "setattr",
                    f"{volume_name}.app",
                    "com.apple.application-instance",
                    attribution_sentinel,
                ]
            )
            subprocess.check_call(["cp", hfs, str(Path(output_dmg).parent)])
            dmg_cmd.append(attribution_sentinel)

        subprocess.check_call(
            dmg_cmd,
            # dmg is seriously chatty
            stdout=subprocess.DEVNULL,
        )
    elif is_osx:
        hybrid = tmpdir / "hybrid.dmg"
        subprocess.check_call(
            [
                "hdiutil",
                "makehybrid",
                "-hfs",
                "-hfs-volume-name",
                volume_name,
                "-hfs-openfolder",
                stagedir,
                "-ov",
                stagedir,
                "-o",
                hybrid,
            ]
        )
        subprocess.check_call(
            [
                "hdiutil",
                "convert",
                "-format",
                "UDBZ",
                "-imagekey",
                "bzip2-level=9",
                "-ov",
                hybrid,
                "-o",
                output_dmg,
            ]
        )


def create_dmg(
    source_directory: Path,
    output_dmg: Path,
    volume_name: str,
    extra_files: List[tuple],
    dmg_tool: Path,
    hfs_tool: Path,
    mkfshfs_tool: Path,
    attribution_sentinel: str = None,
):
    """
    Create a DMG disk image at the path output_dmg from source_directory.

    Use volume_name as the disk image volume name, and
    use extra_files as a list of tuples of (filename, relative path) to copy
    into the disk image.
    """
    if platform.system() not in ("Darwin", "Linux"):
        raise Exception("Don't know how to build a DMG on '%s'" % platform.system())

    with mozfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        stagedir = tmpdir / "stage"
        stagedir.mkdir()

        # Copy the app bundle over using rsync
        rsync(source_directory, stagedir)
        # Copy extra files
        for source, target in extra_files:
            full_target = stagedir / target
            full_target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, full_target)
        if is_linux:
            # Not needed in osx
            generate_hfs_file(stagedir, tmpdir, volume_name, mkfshfs_tool)
        create_app_symlink(stagedir, tmpdir, hfs_tool)
        # Set the folder attributes to use a custom icon
        set_folder_icon(stagedir, tmpdir, hfs_tool)
        chmod(stagedir)
        create_dmg_from_staged(
            stagedir,
            output_dmg,
            tmpdir,
            volume_name,
            hfs_tool,
            dmg_tool,
            mkfshfs_tool,
            attribution_sentinel,
        )


def extract_dmg_contents(
    dmgfile: Path,
    destdir: Path,
    dmg_tool: Path = None,
    hfs_tool: Path = None,
):
    if is_linux:
        with mozfile.TemporaryDirectory() as tmpdir:
            hfs_file = os.path.join(tmpdir, "firefox.hfs")
            subprocess.check_call(
                [dmg_tool, "extract", dmgfile, hfs_file],
                # dmg is seriously chatty
                stdout=subprocess.DEVNULL,
            )
            subprocess.check_call([hfs_tool, hfs_file, "extractall", "/", destdir])
    else:
        # TODO: find better way to resolve topsrcdir (checkout directory)
        topsrcdir = Path(__file__).parent.parent.parent.parent.resolve()
        unpack_diskimage = topsrcdir / "build/package/mac_osx/unpack-diskimage"
        unpack_mountpoint = Path("/tmp/app-unpack")
        subprocess.check_call([unpack_diskimage, dmgfile, unpack_mountpoint, destdir])


def extract_dmg(
    dmgfile: Path,
    output: Path,
    dmg_tool: Path = None,
    hfs_tool: Path = None,
    dsstore: Path = None,
    icon: Path = None,
    background: Path = None,
):
    if platform.system() not in ("Darwin", "Linux"):
        raise Exception("Don't know how to extract a DMG on '%s'" % platform.system())

    with mozfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        extract_dmg_contents(dmgfile, tmpdir, dmg_tool, hfs_tool)
        applications_symlink = tmpdir / " "
        if applications_symlink.is_symlink():
            # Rsync will fail on the presence of this symlink
            applications_symlink.unlink()
        rsync(tmpdir, output)

        if dsstore:
            dsstore.parent.mkdir(parents=True, exist_ok=True)
            rsync(tmpdir / ".DS_Store", dsstore)
        if background:
            background.parent.mkdir(parents=True, exist_ok=True)
            rsync(tmpdir / ".background" / background.name, background)
        if icon:
            icon.parent.mkdir(parents=True, exist_ok=True)
            rsync(tmpdir / ".VolumeIcon.icns", icon)
