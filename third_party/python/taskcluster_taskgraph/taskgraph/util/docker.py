# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import functools
import hashlib
import io
import os
import re
from typing import Optional

from taskgraph.util.archive import create_tar_gz_from_files

IMAGE_DIR = os.path.join(".", "taskcluster", "docker")

from .yaml import load_yaml


def docker_image(name: str, by_tag: bool = False) -> Optional[str]:
    """
    Resolve in-tree prebuilt docker image to ``<registry>/<repository>@sha256:<digest>``,
    or ``<registry>/<repository>:<tag>`` if `by_tag` is `True`.

    Args:
        name (str): The image to build.
        by_tag (bool): If True, will apply a tag based on VERSION file.
            Otherwise will apply a hash based on HASH file.
    Returns:
        Optional[str]: Image if it can be resolved, otherwise None.
    """
    try:
        with open(os.path.join(IMAGE_DIR, name, "REGISTRY")) as f:
            registry = f.read().strip()
    except OSError:
        try:
            with open(os.path.join(IMAGE_DIR, "REGISTRY")) as f:
                registry = f.read().strip()
        except OSError:
            return None

    if not by_tag:
        hashfile = os.path.join(IMAGE_DIR, name, "HASH")
        try:
            with open(hashfile) as f:
                return f"{registry}/{name}@{f.read().strip()}"
        except OSError:
            return None

    try:
        with open(os.path.join(IMAGE_DIR, name, "VERSION")) as f:
            tag = f.read().strip()
    except OSError:
        tag = "latest"
    return f"{registry}/{name}:{tag}"


class VoidWriter:
    """A file object with write capabilities that does nothing with the written
    data."""

    def write(self, buf):
        pass


def generate_context_hash(topsrcdir, image_path, args=None):
    """Generates a sha256 hash for context directory used to build an image."""

    return stream_context_tar(topsrcdir, image_path, VoidWriter(), args=args)


class HashingWriter:
    """A file object with write capabilities that hashes the written data at
    the same time it passes down to a real file object."""

    def __init__(self, writer):
        self._hash = hashlib.sha256()
        self._writer = writer

    def write(self, buf):
        self._hash.update(buf)
        self._writer.write(buf)

    def hexdigest(self):
        return self._hash.hexdigest()


def create_context_tar(topsrcdir, context_dir, out_path, args=None):
    """Create a context tarball.

    A directory ``context_dir`` containing a Dockerfile will be assembled into
    a gzipped tar file at ``out_path``.

    We also scan the source Dockerfile for special syntax that influences
    context generation.

    If a line in the Dockerfile has the form ``# %include <path>``,
    the relative path specified on that line will be matched against
    files in the source repository and added to the context under the
    path ``topsrcdir/``. If an entry is a directory, we add all files
    under that directory.

    If a line in the Dockerfile has the form ``# %ARG <name>``, occurrences of
    the string ``$<name>`` in subsequent lines are replaced with the value
    found in the ``args`` argument. Exception: this doesn't apply to VOLUME
    definitions.

    Returns the SHA-256 hex digest of the created archive.
    """
    with open(out_path, "wb") as fh:
        return stream_context_tar(
            topsrcdir,
            context_dir,
            fh,
            image_name=os.path.basename(out_path),
            args=args,
        )


RUN_TASK_ROOT = os.path.join(os.path.dirname(os.path.dirname(__file__)), "run-task")
RUN_TASK_FILES = {
    f"run-task/{path}": os.path.join(RUN_TASK_ROOT, path)
    for path in [
        "run-task",
        "fetch-content",
        "hgrc",
        "robustcheckout.py",
    ]
}
RUN_TASK_SNIPPET = [
    "COPY run-task/run-task /usr/local/bin/run-task\n",
    "COPY run-task/fetch-content /usr/local/bin/fetch-content\n",
    "COPY run-task/robustcheckout.py /usr/local/mercurial/robustcheckout.py\n"
    "COPY run-task/hgrc /etc/mercurial/hgrc.d/mozilla.rc\n",
]


def stream_context_tar(topsrcdir, context_dir, out_file, image_name=None, args=None):
    """Like create_context_tar, but streams the tar file to the `out_file` file
    object."""
    archive_files = {}
    replace = []
    content = []

    topsrcdir = os.path.abspath(topsrcdir)
    context_dir = os.path.join(topsrcdir, context_dir)

    for root, dirs, files in os.walk(context_dir):
        for f in files:
            source_path = os.path.join(root, f)
            archive_path = source_path[len(context_dir) + 1 :]
            archive_files[archive_path] = open(source_path, "rb")

    # Parse Dockerfile for special syntax of extra files to include.
    content = []
    with open(os.path.join(context_dir, "Dockerfile")) as fh:
        for line in fh:
            if line.startswith("# %ARG"):
                p = line[len("# %ARG ") :].strip()
                if not args or p not in args:
                    raise Exception(f"missing argument: {p}")
                replace.append((re.compile(rf"\${p}\b"), args[p]))
                continue

            for regexp, s in replace:
                line = re.sub(regexp, s, line)

            content.append(line)

            if not line.startswith("# %include"):
                continue

            if line.strip() == "# %include-run-task":
                content.extend(RUN_TASK_SNIPPET)
                archive_files.update(RUN_TASK_FILES)
                continue

            p = line[len("# %include ") :].strip()
            if os.path.isabs(p):
                raise Exception(f"extra include path cannot be absolute: {p}")

            fs_path = os.path.normpath(os.path.join(topsrcdir, p))
            # Check for filesystem traversal exploits.
            if not fs_path.startswith(topsrcdir):
                raise Exception(f"extra include path outside topsrcdir: {p}")

            if not os.path.exists(fs_path):
                raise Exception(f"extra include path does not exist: {p}")

            if os.path.isdir(fs_path):
                for root, dirs, files in os.walk(fs_path):
                    for f in files:
                        source_path = os.path.join(root, f)
                        rel = source_path[len(fs_path) + 1 :]
                        archive_path = os.path.join("topsrcdir", p, rel)
                        archive_files[archive_path] = source_path
            else:
                archive_path = os.path.join("topsrcdir", p)
                archive_files[archive_path] = fs_path

    archive_files["Dockerfile"] = io.BytesIO("".join(content).encode("utf-8"))

    writer = HashingWriter(out_file)
    create_tar_gz_from_files(writer, archive_files, image_name)
    return writer.hexdigest()


@functools.lru_cache(maxsize=None)
def image_paths():
    """Return a map of image name to paths containing their Dockerfile."""
    config = load_yaml("taskcluster", "kinds", "docker-image", "kind.yml")
    return {
        k: os.path.join(IMAGE_DIR, v.get("definition", k))
        for k, v in config["tasks"].items()
    }


def image_path(name):
    paths = image_paths()
    if name in paths:
        return paths[name]
    return os.path.join(IMAGE_DIR, name)


@functools.lru_cache(maxsize=None)
def parse_volumes(image):
    """Parse VOLUME entries from a Dockerfile for an image."""
    volumes = set()

    path = image_path(image)

    with open(os.path.join(path, "Dockerfile"), "rb") as fh:
        for line in fh:
            line = line.strip()
            # We assume VOLUME definitions don't use %ARGS.
            if not line.startswith(b"VOLUME "):
                continue

            v = line.split(None, 1)[1]
            if v.startswith(b"["):
                raise ValueError(
                    "cannot parse array syntax for VOLUME; "
                    "convert to multiple entries"
                )

            volumes |= {volume.decode("utf-8") for volume in v.split()}

    return volumes
