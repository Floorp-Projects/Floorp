# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import datetime
import os
import shutil
import subprocess
import tarfile
import tempfile
from email.utils import format_datetime
from pathlib import Path
from string import Template

import mozpack.path as mozpath
from mozpack.files import FileFinder

from mozbuild.repackaging.application_ini import get_application_ini_values


class NoPackageFound(Exception):
    """Raised when no .deb is found after calling dpkg-buildpackage"""


_DEB_ARCH = {
    "x86": "i386",
    "x86_64": "amd64",
}
# At the moment the Firefox build baseline is jessie.
# The debian-repackage image defined in taskcluster/docker/debian-repackage/Dockerfile
# bootstraps the /srv/jessie-i386 and /srv/jessie-amd64 chroot environments we use to
# create the `.deb` repackages. By running the repackage using chroot we generate shared
# library dependencies that match the Firefox build baseline
# defined in taskcluster/scripts/misc/build-sysroot.sh
_DEB_DIST = "jessie"


def repackage_deb(infile, output, template_dir, arch):

    if not tarfile.is_tarfile(infile):
        raise Exception("Input file %s is not a valid tarfile." % infile)

    deb_arch = _DEB_ARCH[arch]

    if os.path.isdir(f"/srv/{_DEB_DIST}-{deb_arch}"):
        tmpdir = tempfile.mkdtemp(dir=f"/srv/{_DEB_DIST}-{deb_arch}/tmp")
    else:
        tmpdir = tempfile.mkdtemp()

    extract_dir = os.path.join(tmpdir, "source")
    try:
        with tarfile.open(infile) as tar:
            tar.extractall(path=extract_dir)
        finder = FileFinder(extract_dir)
        values = get_application_ini_values(
            finder,
            dict(section="App", value="Name"),
            dict(section="App", value="CodeName", fallback="Name"),
            dict(section="App", value="Vendor"),
            dict(section="App", value="RemotingName"),
            dict(section="App", value="Version"),
            dict(section="App", value="BuildID"),
        )
        app_name = next(values)
        displayname = next(values)
        vendor = next(values)
        remotingname = next(values)
        version = next(values)
        buildid = next(values)
        timestamp = datetime.datetime.strptime(buildid, "%Y%m%d%H%M%S")

        os.mkdir(mozpath.join(extract_dir, "debian"))
        shutil.copy(
            mozpath.join(template_dir, "rules"),
            mozpath.join(extract_dir, "debian", "rules"),
        )
        defines = {
            "DEB_DESCRIPTION": f"{vendor} {displayname}",
            "DEB_PKG_NAME": remotingname.lower(),
            "DEB_PKG_VERSION": f"{version}.{buildid}",
            "DEB_CHANGELOG_DATE": format_datetime(timestamp),
        }

        template_dir_filenames = os.listdir(template_dir)
        template_filenames = [
            filename for filename in template_dir_filenames if filename.endswith(".in")
        ]
        plain_filenames = [
            filename
            for filename in template_dir_filenames
            if not filename.endswith(".in")
        ]

        for filename in template_filenames:
            with open(mozpath.join(template_dir, filename)) as f:
                template = Template(f.read())
            with open(
                mozpath.join(extract_dir, "debian", Path(filename).stem), "w"
            ) as f:
                f.write(template.substitute(defines))

        for filename in plain_filenames:
            shutil.copy(
                mozpath.join(template_dir, filename),
                mozpath.join(extract_dir, "debian"),
            )

        with open(
            mozpath.join(extract_dir, app_name.lower(), "is-packaged-app"), "w"
        ) as f:
            f.write("This is a packaged app.\n")

        if os.path.isdir(f"/srv/{_DEB_DIST}-{deb_arch}"):
            subprocess.check_call(
                [
                    "chroot",
                    f"/srv/{_DEB_DIST}-{deb_arch}",
                    "bash",
                    "-c",
                    f"cd /tmp/*/source; dpkg-buildpackage -us -uc -b -a{deb_arch}",
                ]
            )
        else:
            # If there's is no _DEB_DIST bootstrapped in /srv run the packaging here.
            subprocess.check_call(
                ["dpkg-buildpackage", "-us", "-uc", "-b", f"-a{deb_arch}"],
                cwd=extract_dir,
            )

        deb_file_name = (
            f"{defines['DEB_PKG_NAME']}_{defines['DEB_PKG_VERSION']}_{deb_arch}.deb"
        )
        deb_file_path = mozpath.join(tmpdir, deb_file_name)

        if not os.path.exists(deb_file_path):
            raise NoPackageFound(
                f"No {deb_file_name} package found after calling dpkg-buildpackage"
            )

        subprocess.check_call(["dpkg-deb", "--info", deb_file_path])

        shutil.move(deb_file_path, output)

    finally:
        shutil.rmtree(tmpdir)
