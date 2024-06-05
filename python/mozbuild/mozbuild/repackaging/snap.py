# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# vim: set expandtab tabstop=4 shiftwidth=4:

import os
import shutil
import subprocess
from urllib.request import urlretrieve

from mozbuild.repackaging.application_ini import get_application_ini_values
from mozbuild.repackaging.snapcraft_transform import (
    DesktopFileTransform,
    SnapcraftTransform,
)

UPSTREAM = "https://github.com/canonical/firefox-snap/raw/{}/"
DEPS = [
    "snap/hooks/disconnect-plug-host-hunspell",
    "snap/hooks/post-refresh",
    "firefox.launcher",
    "patch-default-profile.py",
]


def repackage_snap(
    srcdir,
    objdir,
    snapdir,
    snapcraft,
    appname,
    branchname="nightly",
    arch="amd64",
    dry_run=False,
):
    pkgsrc = os.path.join(snapdir, "source", "usr", "lib", "firefox")
    os.path.join(pkgsrc, "distribution")

    upstream_repo = UPSTREAM.format(branchname)

    # Obtain the build's version info
    version, buildno = get_application_ini_values(
        pkgsrc,
        dict(section="App", value="Version"),
        dict(section="App", value="BuildID"),
    )

    for dep in DEPS:
        dep_target = os.path.join(snapdir, dep)
        os.makedirs(os.path.dirname(dep_target), exist_ok=True)
        if not os.path.isfile(dep_target):
            urlretrieve(os.path.join(upstream_repo, dep), dep_target)

    os.chmod(os.path.join(snapdir, "firefox.launcher"), 0o0777)

    shutil.copy(os.path.join(objdir, "dist", "bin", "geckodriver"), pkgsrc)

    shutil.copy(
        os.path.join(srcdir, "browser/branding/nightly/default256.png"), snapdir
    )
    source_desktop = os.path.join(
        srcdir, "taskcluster/docker/firefox-snap/firefox.desktop"
    )
    with open(os.path.join(snapdir, "firefox.desktop"), "w") as desktop_file:
        desktop_file.write(
            DesktopFileTransform(source_desktop, icon="default256.png").repack()
        )

    source_yaml = os.path.join(snapdir, "original.snapcraft.yaml")
    if not os.path.isfile(source_yaml):
        urlretrieve(os.path.join(upstream_repo, "snapcraft.yaml"), source_yaml)
    with open(os.path.join(snapdir, "snapcraft.yaml"), "w") as build_yaml:
        build_yaml.write(
            SnapcraftTransform(source_yaml, appname, version, buildno).repack()
        )

    if dry_run:
        return snapdir

    # At last, build the snap.
    env = os.environ.copy()

    # Note that if snapcraft is run under snap then it will overwrite
    # this env var, but we still need to know `arch` to predict the
    # output file name below, and the env var might also be needed if
    # running a non-snap install of snapcraft.
    env["SNAP_ARCH"] = arch
    subprocess.check_call(
        [snapcraft, "clean", "--use-lxd"],
        env=env,
        cwd=snapdir,
    )
    subprocess.check_call(
        [snapcraft, "--use-lxd"],
        env=env,
        cwd=snapdir,
    )

    snapfile = f"{appname}_{version}-{buildno}_{arch}.snap"
    snappath = os.path.join(snapdir, snapfile)

    if not os.path.exists(snappath):
        raise AssertionError(f"Snap file {snapfile} doesn't exist?")

    # Create a symlink to the file for later use.
    latest_snap = os.path.join(snapdir, "latest.snap")
    try:
        os.unlink(latest_snap)
    except FileNotFoundError:
        pass
    os.symlink(snapfile, latest_snap)

    return snappath


def unpack_tarball(package, destdir):
    os.makedirs(destdir, exist_ok=True)
    subprocess.check_call(
        [
            "tar",
            "-C",
            destdir,
            "-xvf",
            package,
            "--strip-components=1",
        ]
    )


def missing_connections(app_name):
    """
    Parsing the output of `snap connections` e.g.,

    $ snap connections firefox
    Interface               Connecteur                      Prise                           Notes
    alsa                    firefox:alsa                    -                               -
    audio-playback          firefox:audio-playback          :audio-playback                 -
    audio-record            firefox:audio-record            :audio-record                   -
    avahi-observe           firefox:avahi-observe           :avahi-observe                  -
    """
    rv = []
    with subprocess.Popen(
        ["snap", "connections", app_name],
        stdout=subprocess.PIPE,
        encoding="utf-8",
    ) as proc:
        next(proc.stdout)  # skip header
        for line in proc.stdout:
            iface, plug, slot, notes = line.split(maxsplit=3)
            if plug != "-" and slot == "-":
                rv.append(plug)
    return rv
