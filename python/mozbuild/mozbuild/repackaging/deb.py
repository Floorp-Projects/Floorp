# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import os
import shutil
import subprocess
import tarfile
import tempfile
from email.utils import format_datetime
from pathlib import Path
from string import Template

import mozfile
import mozpack.path as mozpath
from mozilla_version.gecko import GeckoVersion

from mozbuild.repackaging.application_ini import get_application_ini_values


class NoDebPackageFound(Exception):
    """Raised when no .deb is found after calling dpkg-buildpackage"""

    def __init__(self, deb_file_path) -> None:
        super().__init__(
            f"No {deb_file_path} package found after calling dpkg-buildpackage"
        )


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


def repackage_deb(infile, output, template_dir, arch, version, build_number):

    if not tarfile.is_tarfile(infile):
        raise Exception("Input file %s is not a valid tarfile." % infile)

    deb_arch = _DEB_ARCH[arch]

    if _is_chroot_available(arch):
        tmpdir = tempfile.mkdtemp(dir=f"/srv/{_DEB_DIST}-{deb_arch}/tmp")
    else:
        tmpdir = tempfile.mkdtemp()

    source_dir = os.path.join(tmpdir, "source")
    try:
        mozfile.extract_tarball(infile, source_dir)
        application_ini_data = _extract_application_ini_data(source_dir)
        build_variables = _get_build_variables(
            application_ini_data, arch, version, build_number
        )

        _copy_plain_deb_config(template_dir, source_dir)
        _render_deb_templates(template_dir, source_dir, build_variables)

        app_name = application_ini_data["name"]
        with open(
            mozpath.join(source_dir, app_name.lower(), "is-packaged-app"), "w"
        ) as f:
            f.write("This is a packaged app.\n")

        _inject_deb_distribution_folder(source_dir, app_name)
        _generate_deb_archive(
            source_dir,
            target_dir=tmpdir,
            output_file_path=output,
            build_variables=build_variables,
            arch=arch,
        )

    finally:
        shutil.rmtree(tmpdir)


def _extract_application_ini_data(application_director):
    values = get_application_ini_values(
        application_director,
        dict(section="App", value="Name"),
        dict(section="App", value="CodeName", fallback="Name"),
        dict(section="App", value="Vendor"),
        dict(section="App", value="RemotingName"),
        dict(section="App", value="BuildID"),
    )

    data = {
        "name": next(values),
        "display_name": next(values),
        "vendor": next(values),
        "remoting_name": next(values),
        "build_id": next(values),
    }
    data["timestamp"] = datetime.datetime.strptime(data["build_id"], "%Y%m%d%H%M%S")

    return data


def _get_build_variables(
    application_ini_data,
    arch,
    version_string,
    build_number,
):
    version = GeckoVersion.parse(version_string)
    # Nightlies don't have build numbers
    deb_pkg_version = (
        f"{version}~{application_ini_data['build_id']}"
        if version.is_nightly
        else f"{version}~build{build_number}"
    )

    return {
        "DEB_DESCRIPTION": f"{application_ini_data['vendor']} {application_ini_data['display_name']}",
        "DEB_PKG_NAME": application_ini_data["remoting_name"].lower(),
        "DEB_PKG_VERSION": deb_pkg_version,
        "DEB_CHANGELOG_DATE": format_datetime(application_ini_data["timestamp"]),
        "DEB_ARCH_NAME": _DEB_ARCH[arch],
    }


def _copy_plain_deb_config(input_template_dir, source_dir):
    template_dir_filenames = os.listdir(input_template_dir)
    plain_filenames = [
        mozpath.basename(filename)
        for filename in template_dir_filenames
        if not filename.endswith(".in")
    ]
    os.makedirs(mozpath.join(source_dir, "debian"), exist_ok=True)

    for filename in plain_filenames:
        shutil.copy(
            mozpath.join(input_template_dir, filename),
            mozpath.join(source_dir, "debian", filename),
        )


def _render_deb_templates(input_template_dir, source_dir, build_variables):
    template_dir_filenames = os.listdir(input_template_dir)
    template_filenames = [
        mozpath.basename(filename)
        for filename in template_dir_filenames
        if filename.endswith(".in")
    ]
    os.makedirs(mozpath.join(source_dir, "debian"), exist_ok=True)

    for file_name in template_filenames:
        with open(mozpath.join(input_template_dir, file_name)) as f:
            template = Template(f.read())
        with open(mozpath.join(source_dir, "debian", Path(file_name).stem), "w") as f:
            f.write(template.substitute(build_variables))


def _inject_deb_distribution_folder(source_dir, app_name):
    with tempfile.TemporaryDirectory() as git_clone_dir:
        subprocess.check_call(
            [
                "git",
                "clone",
                "https://github.com/mozilla-partners/deb.git",
                git_clone_dir,
            ],
        )
        shutil.copytree(
            mozpath.join(git_clone_dir, "desktop/deb/distribution"),
            mozpath.join(source_dir, app_name.lower(), "distribution"),
        )


def _generate_deb_archive(
    source_dir, target_dir, output_file_path, build_variables, arch
):
    command = _get_command(arch)
    subprocess.check_call(command, cwd=source_dir)
    deb_arch = _DEB_ARCH[arch]
    deb_file_name = f"{build_variables['DEB_PKG_NAME']}_{build_variables['DEB_PKG_VERSION']}_{deb_arch}.deb"
    deb_file_path = mozpath.join(target_dir, deb_file_name)

    if not os.path.exists(deb_file_path):
        raise NoDebPackageFound(deb_file_path)

    subprocess.check_call(["dpkg-deb", "--info", deb_file_path])
    shutil.move(deb_file_path, output_file_path)


def _get_command(arch):
    deb_arch = _DEB_ARCH[arch]
    command = [
        "dpkg-buildpackage",
        # TODO: Use long options once we stop supporting Debian Jesse. They're more
        # explicit.
        #
        # Long options were added in dpkg 1.18.8 which is part of Debian Stretch.
        #
        # https://git.dpkg.org/cgit/dpkg/dpkg.git/commit/?h=1.18.x&id=293bd243a19149165fc4fd8830b16a51d471a5e9
        # https://packages.debian.org/stretch/dpkg-dev
        "-us",  # --unsigned-source
        "-uc",  # --unsigned-changes
        "-b",  # --build=binary
        f"--host-arch={deb_arch}",
    ]

    if _is_chroot_available(arch):
        flattened_command = " ".join(command)
        command = [
            "chroot",
            f"/srv/{_DEB_DIST}-{deb_arch}",
            "bash",
            "-c",
            f"cd /tmp/*/source; {flattened_command}",
        ]

    return command


def _is_chroot_available(arch):
    deb_arch = _DEB_ARCH[arch]
    return os.path.isdir(f"/srv/{_DEB_DIST}-{deb_arch}")
