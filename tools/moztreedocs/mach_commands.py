# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals


import fnmatch
import multiprocessing
import os
import subprocess
import sys
import time
import yaml
import uuid
import mozpack.path as mozpath
import sentry_sdk

from functools import partial
from pprint import pprint

from mach.registrar import Registrar
from mozbuild.util import memoize
from mach.decorators import (
    Command,
    CommandArgument,
    SubCommand,
)

here = os.path.abspath(os.path.dirname(__file__))
topsrcdir = os.path.abspath(os.path.dirname(os.path.dirname(here)))
DOC_ROOT = os.path.join(topsrcdir, "docs")
BASE_LINK = "http://gecko-docs.mozilla.org-l1.s3-website.us-west-2.amazonaws.com/"


# Helps manage in-tree documentation.


@Command(
    "doc",
    category="devenv",
    virtualenv_name="docs",
    description="Generate and serve documentation from the tree.",
)
@CommandArgument(
    "path",
    default=None,
    metavar="DIRECTORY",
    nargs="?",
    help="Path to documentation to build and display.",
)
@CommandArgument(
    "--format", default="html", dest="fmt", help="Documentation format to write."
)
@CommandArgument(
    "--outdir", default=None, metavar="DESTINATION", help="Where to write output."
)
@CommandArgument(
    "--archive",
    action="store_true",
    help="Write a gzipped tarball of generated docs.",
)
@CommandArgument(
    "--no-open",
    dest="auto_open",
    default=True,
    action="store_false",
    help="Don't automatically open HTML docs in a browser.",
)
@CommandArgument(
    "--no-serve",
    dest="serve",
    default=True,
    action="store_false",
    help="Don't serve the generated docs after building.",
)
@CommandArgument(
    "--http",
    default="localhost:5500",
    metavar="ADDRESS",
    help="Serve documentation on the specified host and port, "
    'default "localhost:5500".',
)
@CommandArgument("--upload", action="store_true", help="Upload generated files to S3.")
@CommandArgument(
    "-j",
    "--jobs",
    default=str(multiprocessing.cpu_count()),
    dest="jobs",
    help="Distribute the build over N processes in parallel.",
)
@CommandArgument("--write-url", default=None, help="Write S3 Upload URL to text file")
@CommandArgument(
    "--linkcheck", action="store_true", help="Check if the links are still valid"
)
@CommandArgument("--verbose", action="store_true", help="Run Sphinx in verbose mode")
def build_docs(
    command_context,
    path=None,
    fmt="html",
    outdir=None,
    auto_open=True,
    serve=True,
    http=None,
    archive=False,
    upload=False,
    jobs=None,
    write_url=None,
    linkcheck=None,
    verbose=None,
):

    # TODO: Bug 1704891 - move the ESLint setup tools to a shared place.
    sys.path.append(mozpath.join(command_context.topsrcdir, "tools", "lint", "eslint"))
    import setup_helper

    setup_helper.set_project_root(command_context.topsrcdir)

    if not setup_helper.check_node_executables_valid():
        return 1

    setup_helper.eslint_maybe_setup()

    # Set the path so that Sphinx can find jsdoc, unfortunately there isn't
    # a way to pass this to Sphinx itself at the moment.
    os.environ["PATH"] = (
        mozpath.join(command_context.topsrcdir, "node_modules", ".bin")
        + os.pathsep
        + _node_path()
        + os.pathsep
        + os.environ["PATH"]
    )

    command_context.activate_virtualenv()
    command_context.virtualenv_manager.install_pip_requirements(
        os.path.join(here, "requirements.txt")
    )

    import webbrowser
    from livereload import Server
    from moztreedocs.package import create_tarball

    unique_id = "%s/%s" % (project(), str(uuid.uuid1()))

    outdir = outdir or os.path.join(command_context.topobjdir, "docs")
    savedir = os.path.join(outdir, fmt)

    path = path or command_context.topsrcdir
    path = os.path.normpath(os.path.abspath(path))

    docdir = _find_doc_dir(path)
    if not docdir:
        print(_dump_sphinx_backtrace())
        return die(
            "failed to generate documentation:\n"
            "%s: could not find docs at this location" % path
        )

    if linkcheck:
        # We want to verify if the links are valid or not
        fmt = "linkcheck"

    result = _run_sphinx(docdir, savedir, fmt=fmt, jobs=jobs, verbose=verbose)
    if result != 0:
        print(_dump_sphinx_backtrace())
        return die(
            "failed to generate documentation:\n"
            "%s: sphinx return code %d" % (path, result)
        )
    else:
        print("\nGenerated documentation:\n%s" % savedir)

    # Upload the artifact containing the link to S3
    # This would be used by code-review to post the link to Phabricator
    if write_url is not None:
        unique_link = BASE_LINK + unique_id + "/index.html"
        with open(write_url, "w") as fp:
            fp.write(unique_link)
            fp.flush()
        print("Generated " + write_url)

    if archive:
        archive_path = os.path.join(outdir, "%s.tar.gz" % project())
        create_tarball(archive_path, savedir)
        print("Archived to %s" % archive_path)

    if upload:
        _s3_upload(savedir, project(), unique_id, version())

    if not serve:
        index_path = os.path.join(savedir, "index.html")
        if auto_open and os.path.isfile(index_path):
            webbrowser.open(index_path)
        return

    # Create livereload server. Any files modified in the specified docdir
    # will cause a re-build and refresh of the browser (if open).
    try:
        host, port = http.split(":", 1)
        port = int(port)
    except ValueError:
        return die("invalid address: %s" % http)

    server = Server()

    sphinx_trees = manager().trees or {savedir: docdir}
    for _, src in sphinx_trees.items():
        run_sphinx = partial(
            _run_sphinx, src, savedir, fmt=fmt, jobs=jobs, verbose=verbose
        )
        server.watch(src, run_sphinx)
    server.serve(
        host=host,
        port=port,
        root=savedir,
        open_url_delay=0.1 if auto_open else None,
    )


def _dump_sphinx_backtrace():
    """
    If there is a sphinx dump file, read and return
    its content.
    By default, it isn't displayed.
    """
    pattern = "sphinx-err-*"
    output = ""
    tmpdir = "/tmp"

    if not os.path.isdir(tmpdir):
        # Only run it on Linux
        return
    files = os.listdir(tmpdir)
    for name in files:
        if fnmatch.fnmatch(name, pattern):
            pathFile = os.path.join(tmpdir, name)
            stat = os.stat(pathFile)
            output += "Name: {0} / Creation date: {1}\n".format(
                pathFile, time.ctime(stat.st_mtime)
            )
            with open(pathFile) as f:
                output += f.read()
    return output


def _run_sphinx(docdir, savedir, config=None, fmt="html", jobs=None, verbose=None):
    import sphinx.cmd.build

    config = config or manager().conf_py_path
    # When running sphinx with sentry, it adds significant overhead
    # and makes the build generation very very very slow
    # So, disable it to generate the doc faster
    sentry_sdk.init(None)
    args = [
        "-T",
        "-b",
        fmt,
        "-c",
        os.path.dirname(config),
        docdir,
        savedir,
    ]
    if jobs:
        args.extend(["-j", jobs])
    if verbose:
        args.extend(["-v", "-v"])
    print("Run sphinx with:")
    print(args)
    return sphinx.cmd.build.build_main(args)


def manager():
    from moztreedocs import manager

    return manager


@memoize
def _read_project_properties():
    import imp

    path = os.path.normpath(manager().conf_py_path)
    with open(path, "r") as fh:
        conf = imp.load_module("doc_conf", fh, path, (".py", "r", imp.PY_SOURCE))

    # Prefer the Mozilla project name, falling back to Sphinx's
    # default variable if it isn't defined.
    project = getattr(conf, "moz_project_name", None)
    if not project:
        project = conf.project.replace(" ", "_")

    return {"project": project, "version": getattr(conf, "version", None)}


def project():
    return _read_project_properties()["project"]


def version():
    return _read_project_properties()["version"]


def _node_path():
    from mozbuild.nodeutil import find_node_executable

    node, _ = find_node_executable()

    return os.path.dirname(node)


def _find_doc_dir(path):
    if os.path.isfile(path):
        return

    valid_doc_dirs = ("doc", "docs")
    if os.path.basename(path) in valid_doc_dirs:
        return path

    for d in valid_doc_dirs:
        p = os.path.join(path, d)
        if os.path.isdir(p):
            return p


def _s3_upload(root, project, unique_id, version=None):
    from moztreedocs.package import distribution_files
    from moztreedocs.upload import s3_upload, s3_set_redirects

    # Workaround the issue
    # BlockingIOError: [Errno 11] write could not complete without blocking
    # https://github.com/travis-ci/travis-ci/issues/8920
    import fcntl

    fcntl.fcntl(1, fcntl.F_SETFL, 0)

    # Files are uploaded to multiple locations:
    #
    # <project>/latest
    # <project>/<version>
    #
    # This allows multiple projects and versions to be stored in the
    # S3 bucket.

    files = list(distribution_files(root))
    key_prefixes = []
    if version:
        key_prefixes.append("%s/%s" % (project, version))

    # Until we redirect / to main/latest, upload the main docs
    # to the root.
    if project == "main":
        key_prefixes.append("")

    key_prefixes.append(unique_id)

    with open(os.path.join(DOC_ROOT, "config.yml"), "r") as fh:
        redirects = yaml.safe_load(fh)["redirects"]

    redirects = {k.strip("/"): v.strip("/") for k, v in redirects.items()}

    all_redirects = {}

    for prefix in key_prefixes:
        s3_upload(files, prefix)

        # Don't setup redirects for the "version" or "uuid" prefixes since
        # we are exceeding a 50 redirect limit and external things are
        # unlikely to link there anyway (see bug 1614908).
        if (version and prefix.endswith(version)) or prefix == unique_id:
            continue

        if prefix:
            prefix += "/"
        all_redirects.update({prefix + k: prefix + v for k, v in redirects.items()})

    print("Redirects currently staged")
    pprint(all_redirects, indent=1)

    s3_set_redirects(all_redirects)

    unique_link = BASE_LINK + unique_id + "/index.html"
    print("Uploaded documentation can be accessed here " + unique_link)


@SubCommand(
    "doc",
    "mach-telemetry",
    description="Generate documentation from Glean metrics.yaml files",
)
def generate_telemetry_docs(command_context):
    args = [
        sys.executable,
        "-m" "glean_parser",
        "translate",
        "-f",
        "markdown",
        "-o",
        os.path.join(topsrcdir, "python/mach/docs/"),
        os.path.join(topsrcdir, "python/mach/pings.yaml"),
        os.path.join(topsrcdir, "python/mach/metrics.yaml"),
    ]
    metrics_paths = [
        handler.metrics_path
        for handler in Registrar.command_handlers.values()
        if handler.metrics_path is not None
    ]
    args.extend(
        [os.path.join(command_context.topsrcdir, path) for path in set(metrics_paths)]
    )
    subprocess.check_call(args)


def die(msg, exit_code=1):
    msg = "%s: %s" % (sys.argv[0], msg)
    print(msg, file=sys.stderr)
    return exit_code
