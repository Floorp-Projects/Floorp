# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import hashlib
import io
import json
import os
import re
import sys
import urllib.parse

import requests_unixsocket

from .archive import create_tar_gz_from_files
from .memoize import memoize

IMAGE_DIR = os.path.join(".", "taskcluster", "docker")

from .yaml import load_yaml


def docker_url(path, **kwargs):
    docker_socket = os.environ.get("DOCKER_SOCKET", "/var/run/docker.sock")
    return urllib.parse.urlunparse(
        (
            "http+unix",
            urllib.parse.quote(docker_socket, safe=""),
            path,
            "",
            urllib.parse.urlencode(kwargs),
            "",
        )
    )


def post_to_docker(tar, api_path, **kwargs):
    """POSTs a tar file to a given docker API path.

    The tar argument can be anything that can be passed to requests.post()
    as data (e.g. iterator or file object).
    The extra keyword arguments are passed as arguments to the docker API.
    """
    req = requests_unixsocket.Session().post(
        docker_url(api_path, **kwargs),
        data=tar,
        stream=True,
        headers={"Content-Type": "application/x-tar"},
    )
    if req.status_code != 200:
        message = req.json().get("message")
        if not message:
            message = f"docker API returned HTTP code {req.status_code}"
        raise Exception(message)
    status_line = {}

    buf = b""
    for content in req.iter_content(chunk_size=None):
        if not content:
            continue
        # Sometimes, a chunk of content is not a complete json, so we cumulate
        # with leftovers from previous iterations.
        buf += content
        try:
            data = json.loads(buf)
        except Exception:
            continue
        buf = b""
        # data is sometimes an empty dict.
        if not data:
            continue
        # Mimic how docker itself presents the output. This code was tested
        # with API version 1.18 and 1.26.
        if "status" in data:
            if "id" in data:
                if sys.stderr.isatty():
                    total_lines = len(status_line)
                    line = status_line.setdefault(data["id"], total_lines)
                    n = total_lines - line
                    if n > 0:
                        # Move the cursor up n lines.
                        sys.stderr.write(f"\033[{n}A")
                    # Clear line and move the cursor to the beginning of it.
                    sys.stderr.write("\033[2K\r")
                    sys.stderr.write(
                        "{}: {} {}\n".format(
                            data["id"], data["status"], data.get("progress", "")
                        )
                    )
                    if n > 1:
                        # Move the cursor down n - 1 lines, which, considering
                        # the carriage return on the last write, gets us back
                        # where we started.
                        sys.stderr.write(f"\033[{n - 1}B")
                else:
                    status = status_line.get(data["id"])
                    # Only print status changes.
                    if status != data["status"]:
                        sys.stderr.write("{}: {}\n".format(data["id"], data["status"]))
                        status_line[data["id"]] = data["status"]
            else:
                status_line = {}
                sys.stderr.write("{}\n".format(data["status"]))
        elif "stream" in data:
            sys.stderr.write(data["stream"])
        elif "aux" in data:
            sys.stderr.write(repr(data["aux"]))
        elif "error" in data:
            sys.stderr.write("{}\n".format(data["error"]))
            # Sadly, docker doesn't give more than a plain string for errors,
            # so the best we can do to propagate the error code from the command
            # that failed is to parse the error message...
            errcode = 1
            m = re.search(r"returned a non-zero code: (\d+)", data["error"])
            if m:
                errcode = int(m.group(1))
            sys.exit(errcode)
        else:
            raise NotImplementedError(repr(data))
        sys.stderr.flush()


def docker_image(name, by_tag=False):
    """
    Resolve in-tree prebuilt docker image to ``<registry>/<repository>@sha256:<digest>``,
    or ``<registry>/<repository>:<tag>`` if `by_tag` is `True`.
    """
    try:
        with open(os.path.join(IMAGE_DIR, name, "REGISTRY")) as f:
            registry = f.read().strip()
    except OSError:
        with open(os.path.join(IMAGE_DIR, "REGISTRY")) as f:
            registry = f.read().strip()

    if not by_tag:
        hashfile = os.path.join(IMAGE_DIR, name, "HASH")
        try:
            with open(hashfile) as f:
                return f"{registry}/{name}@{f.read().strip()}"
        except OSError:
            raise Exception(f"Failed to read HASH file {hashfile}")

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
                raise Exception("extra include path cannot be absolute: %s" % p)

            fs_path = os.path.normpath(os.path.join(topsrcdir, p))
            # Check for filesystem traversal exploits.
            if not fs_path.startswith(topsrcdir):
                raise Exception("extra include path outside topsrcdir: %s" % p)

            if not os.path.exists(fs_path):
                raise Exception("extra include path does not exist: %s" % p)

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


@memoize
def image_paths():
    """Return a map of image name to paths containing their Dockerfile."""
    config = load_yaml("taskcluster", "ci", "docker-image", "kind.yml")
    return {
        k: os.path.join(IMAGE_DIR, v.get("definition", k))
        for k, v in config["jobs"].items()
    }


def image_path(name):
    paths = image_paths()
    if name in paths:
        return paths[name]
    return os.path.join(IMAGE_DIR, name)


@memoize
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
