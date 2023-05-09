# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
import zipfile
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
    "all": "all",
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

    tmpdir = _create_temporary_directory(arch)
    source_dir = os.path.join(tmpdir, "source")
    try:
        mozfile.extract_tarball(infile, source_dir)
        application_ini_data = _extract_application_ini_data(infile)
        build_variables = _get_build_variables(
            application_ini_data,
            arch,
            version,
            build_number,
            depends="${shlibs:Depends},",
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


def repackage_deb_l10n(
    input_xpi_file, input_tar_file, output, template_dir, version, build_number
):
    arch = "all"

    tmpdir = _create_temporary_directory(arch)
    source_dir = os.path.join(tmpdir, "source")
    try:
        langpack_metadata = _extract_langpack_metadata(input_xpi_file)
        langpack_dir = mozpath.join(source_dir, "firefox", "distribution", "extensions")
        application_ini_data = _extract_application_ini_data(input_tar_file)
        langpack_id = langpack_metadata["langpack_id"]
        build_variables = _get_build_variables(
            application_ini_data,
            arch,
            version,
            build_number,
            depends=application_ini_data["remoting_name"],
            # Debian package names are only lowercase
            package_name_suffix=f"-l10n-{langpack_id.lower()}",
            description_suffix=f" - {langpack_metadata['description']}",
        )
        _copy_plain_deb_config(template_dir, source_dir)
        _render_deb_templates(
            template_dir, source_dir, build_variables, exclude_file_names=["links.in"]
        )

        os.makedirs(langpack_dir, exist_ok=True)
        shutil.copy(
            input_xpi_file,
            mozpath.join(
                langpack_dir,
                f"{langpack_metadata['browser_specific_settings']['gecko']['id']}.xpi",
            ),
        )
        _generate_deb_archive(
            source_dir=source_dir,
            target_dir=tmpdir,
            output_file_path=output,
            build_variables=build_variables,
            arch=arch,
        )
    finally:
        shutil.rmtree(tmpdir)


def _extract_application_ini_data(input_tar_file):
    with tempfile.TemporaryDirectory() as d:
        with tarfile.open(input_tar_file) as tar:
            application_ini_files = [
                tar_info
                for tar_info in tar.getmembers()
                if tar_info.name.endswith("/application.ini")
            ]
            if len(application_ini_files) == 0:
                raise ValueError(
                    f"Cannot find any application.ini file in archive {input_tar_file}"
                )
            if len(application_ini_files) > 1:
                raise ValueError(
                    f"Too many application.ini files found in archive {input_tar_file}. "
                    f"Found: {application_ini_files}"
                )

            tar.extract(application_ini_files[0], path=d)

        return _extract_application_ini_data_from_directory(d)


def _extract_application_ini_data_from_directory(application_directory):
    values = get_application_ini_values(
        application_directory,
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
    depends,
    package_name_suffix="",
    description_suffix="",
):
    version = GeckoVersion.parse(version_string)
    # Nightlies don't have build numbers
    deb_pkg_version = (
        f"{version}~{application_ini_data['build_id']}"
        if version.is_nightly
        else f"{version}~build{build_number}"
    )
    remoting_name = application_ini_data["remoting_name"].lower()

    return {
        "DEB_DESCRIPTION": f"{application_ini_data['vendor']} {application_ini_data['display_name']}"
        f"{description_suffix}",
        "DEB_PKG_INSTALL_PATH": f"usr/lib/{remoting_name}",
        "DEB_PKG_NAME": f"{remoting_name}{package_name_suffix}",
        "DEB_PKG_VERSION": deb_pkg_version,
        "DEB_CHANGELOG_DATE": format_datetime(application_ini_data["timestamp"]),
        "DEB_ARCH_NAME": _DEB_ARCH[arch],
        "DEB_DEPENDS": depends,
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


def _render_deb_templates(
    input_template_dir, source_dir, build_variables, exclude_file_names=None
):
    exclude_file_names = [] if exclude_file_names is None else exclude_file_names

    template_dir_filenames = os.listdir(input_template_dir)
    template_filenames = [
        mozpath.basename(filename)
        for filename in template_dir_filenames
        if filename.endswith(".in") and filename not in exclude_file_names
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
    ]

    if deb_arch != "all":
        command.append(f"--host-arch={deb_arch}")

    if _is_chroot_available(arch):
        flattened_command = " ".join(command)
        command = [
            "chroot",
            _get_chroot_path(arch),
            "bash",
            "-c",
            f"cd /tmp/*/source; {flattened_command}",
        ]

    return command


def _create_temporary_directory(arch):
    if _is_chroot_available(arch):
        return tempfile.mkdtemp(dir=f"{_get_chroot_path(arch)}/tmp")
    else:
        return tempfile.mkdtemp()


def _is_chroot_available(arch):
    return os.path.isdir(_get_chroot_path(arch))


def _get_chroot_path(arch):
    deb_arch = "amd64" if arch == "all" else _DEB_ARCH[arch]
    return f"/srv/{_DEB_DIST}-{deb_arch}"


_MANIFEST_FILE_NAME = "manifest.json"


def _extract_langpack_metadata(input_xpi_file):
    with tempfile.TemporaryDirectory() as d:
        with zipfile.ZipFile(input_xpi_file) as zip:
            zip.extract(_MANIFEST_FILE_NAME, path=d)

        with open(mozpath.join(d, _MANIFEST_FILE_NAME)) as f:
            return json.load(f)
