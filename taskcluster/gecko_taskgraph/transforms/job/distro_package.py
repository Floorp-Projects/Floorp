# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running spidermonkey jobs via dedicated scripts
"""


import os
import re

import taskgraph
from taskgraph.util.taskcluster import get_root_url
from voluptuous import Any, Optional, Required

from gecko_taskgraph.transforms.job import run_job_using
from gecko_taskgraph.transforms.job.common import add_artifacts

from gecko_taskgraph.util.hash import hash_path
from gecko_taskgraph.util.schema import Schema
from gecko_taskgraph import GECKO

DSC_PACKAGE_RE = re.compile(".*(?=_)")
SOURCE_PACKAGE_RE = re.compile(r".*(?=[-_]\d)")

source_definition = {
    Required("url"): str,
    Required("sha256"): str,
}

common_schema = Schema(
    {
        # URL/SHA256 of a source file to build, which can either be a source
        # control (.dsc), or a tarball.
        Required(Any("dsc", "tarball")): source_definition,
        # Package name. Normally derived from the source control or tarball file
        # name. Use in case the name doesn't match DSC_PACKAGE_RE or
        # SOURCE_PACKAGE_RE.
        Optional("name"): str,
        # Patch to apply to the extracted source.
        Optional("patch"): str,
        # Command to run before dpkg-buildpackage.
        Optional("pre-build-command"): str,
        # Architecture to build the package for.
        Optional("arch"): str,
        # List of package tasks to get build dependencies from.
        Optional("packages"): [str],
        # What resolver to use to install build dependencies. The default
        # (apt-get) is good in most cases, but in subtle cases involving
        # a *-backports archive, its solver might not be able to find a
        # solution that satisfies the build dependencies.
        Optional("resolver"): Any("apt-get", "aptitude"),
        # Base work directory used to set up the task.
        Required("workdir"): str,
    }
)

debian_schema = common_schema.extend(
    {
        Required("using"): "debian-package",
        # Debian distribution
        Required("dist"): str,
    }
)

ubuntu_schema = common_schema.extend(
    {
        Required("using"): "ubuntu-package",
        # Ubuntu distribution
        Required("dist"): str,
    }
)


def common_package(config, job, taskdesc, distro, version):
    run = job["run"]

    name = taskdesc["label"].replace(f"{config.kind}-", "", 1)

    arch = run.get("arch", "amd64")

    worker = taskdesc["worker"]
    worker.setdefault("artifacts", [])

    image = "%s%d" % (distro, version)
    if arch != "amd64":
        image += "-" + arch
    image += "-packages"
    worker["docker-image"] = {"in-tree": image}

    add_artifacts(config, job, taskdesc, path="/tmp/artifacts")

    env = worker.setdefault("env", {})
    env["DEBFULLNAME"] = "Mozilla build team"
    env["DEBEMAIL"] = "dev-builds@lists.mozilla.org"

    if "dsc" in run:
        src = run["dsc"]
        unpack = "dpkg-source -x {src_file} {package}"
        package_re = DSC_PACKAGE_RE
    elif "tarball" in run:
        src = run["tarball"]
        unpack = (
            "mkdir {package} && "
            "tar -C {package} -axf {src_file} --strip-components=1"
        )
        package_re = SOURCE_PACKAGE_RE
    else:
        raise RuntimeError("Unreachable")
    src_url = src["url"]
    src_file = os.path.basename(src_url)
    src_sha256 = src["sha256"]
    package = run.get("name")
    if not package:
        package = package_re.match(src_file).group(0)
    unpack = unpack.format(src_file=src_file, package=package)

    resolver = run.get("resolver", "apt-get")
    if resolver == "apt-get":
        resolver = "apt-get -yyq --no-install-recommends"
    elif resolver == "aptitude":
        resolver = (
            "aptitude -y --without-recommends -o "
            "Aptitude::ProblemResolver::Hints::KeepBuildDeps="
            '"reject {}-build-deps :UNINST"'
        ).format(package)
    else:
        raise RuntimeError("Unreachable")

    adjust = ""
    if "patch" in run:
        # We don't use robustcheckout or run-task to get a checkout. So for
        # this one file we'd need from a checkout, download it.
        env["PATCH_URL"] = config.params.file_url(
            "build/debian-packages/{patch}".format(patch=run["patch"]),
        )
        adjust += "curl -sL $PATCH_URL | patch -p1 && "
    if "pre-build-command" in run:
        adjust += run["pre-build-command"] + " && "
    if "tarball" in run:
        adjust += "mv ../{src_file} ../{package}_{ver}.orig.tar.gz && ".format(
            src_file=src_file,
            package=package,
            ver="$(dpkg-parsechangelog | awk '$1==\"Version:\"{print $2}' | cut -f 1 -d -)",
        )
    if "patch" not in run and "pre-build-command" not in run:
        adjust += (
            'debchange -l ".{prefix}moz" --distribution "{dist}"'
            ' "Mozilla backport for {dist}." < /dev/null && '
        ).format(
            prefix=name.split("-", 1)[0],
            dist=run["dist"],
        )

    worker["command"] = [
        "sh",
        "-x",
        "-c",
        # Add sources for packages coming from other package tasks.
        "/usr/local/sbin/setup_packages.sh {root_url} $PACKAGES && "
        "apt-get update && "
        # Upgrade packages that might have new versions in package tasks.
        "apt-get dist-upgrade && " "cd /tmp && "
        # Get, validate and extract the package source.
        "(dget -d -u {src_url} || exit 100) && "
        'echo "{src_sha256}  {src_file}" | sha256sum -c && '
        "{unpack} && "
        "cd {package} && "
        # Optionally apply patch and/or pre-build command.
        "{adjust}"
        # Install the necessary build dependencies.
        "(cd ..; mk-build-deps -i -r {package}/debian/control -t '{resolver}' || exit 100) && "
        # Build the package
        'DEB_BUILD_OPTIONS="parallel=$(nproc) nocheck" dpkg-buildpackage && '
        # Copy the artifacts
        "mkdir -p {artifacts}/apt && "
        "dcmd cp ../{package}_*.changes {artifacts}/apt/ && "
        "cd {artifacts} && "
        # Make the artifacts directory usable as an APT repository.
        "apt-ftparchive sources apt | gzip -c9 > apt/Sources.gz && "
        "apt-ftparchive packages apt | gzip -c9 > apt/Packages.gz".format(
            root_url=get_root_url(False),
            package=package,
            src_url=src_url,
            src_file=src_file,
            src_sha256=src_sha256,
            unpack=unpack,
            adjust=adjust,
            artifacts="/tmp/artifacts",
            resolver=resolver,
        ),
    ]

    if run.get("packages"):
        env = worker.setdefault("env", {})
        env["PACKAGES"] = {
            "task-reference": " ".join(f"<{p}>" for p in run["packages"])
        }
        deps = taskdesc.setdefault("dependencies", {})
        for p in run["packages"]:
            deps[p] = f"packages-{p}"

    # Use the command generated above as the base for the index hash.
    # We rely on it not varying depending on the head_repository or head_rev.
    digest_data = list(worker["command"])
    if "patch" in run:
        digest_data.append(
            hash_path(os.path.join(GECKO, "build", "debian-packages", run["patch"]))
        )

    if not taskgraph.fast:
        taskdesc["cache"] = {
            "type": "packages.v1",
            "name": name,
            "digest-data": digest_data,
        }


@run_job_using("docker-worker", "debian-package", schema=debian_schema)
def docker_worker_debian_package(config, job, taskdesc):
    run = job["run"]
    version = {
        "wheezy": 7,
        "jessie": 8,
        "stretch": 9,
        "buster": 10,
        "bullseye": 11,
    }[run["dist"]]
    common_package(config, job, taskdesc, "debian", version)


@run_job_using("docker-worker", "ubuntu-package", schema=ubuntu_schema)
def docker_worker_ubuntu_package(config, job, taskdesc):
    run = job["run"]
    version = {
        "bionic": 1804,
        "focal": 2004,
    }[run["dist"]]
    common_package(config, job, taskdesc, "ubuntu", version)
