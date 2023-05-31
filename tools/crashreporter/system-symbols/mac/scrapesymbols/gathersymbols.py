#!/usr/bin/env python
# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import argparse
import concurrent.futures
import datetime
import os
import subprocess
import sys
import traceback
import urllib
import zipfile

import requests

if sys.platform == "darwin":
    SYSTEM_DIRS = [
        "/usr/lib",
        "/System/Library/Frameworks",
        "/System/Library/PrivateFrameworks",
        "/System/Library/Extensions",
    ]
else:
    SYSTEM_DIRS = ["/lib", "/usr/lib"]
SYMBOL_SERVER_URL = "https://symbols.mozilla.org/"


def should_process(f, platform=sys.platform):
    """Determine if a file is a platform binary"""
    if platform == "darwin":
        """
        The 'file' command can error out. One example is "illegal byte
        sequence" on a Japanese language UTF8 text file. So we must wrap the
        command in a try/except block to prevent the script from terminating
        prematurely when this happens.
        """
        try:
            filetype = subprocess.check_output(["file", "-Lb", f], text=True)
        except subprocess.CalledProcessError:
            return False
        """Skip kernel extensions"""
        if "kext bundle" in filetype:
            return False
        return filetype.startswith("Mach-O")
    else:
        return subprocess.check_output(["file", "-Lb", f], text=True).startswith("ELF")
    return False


def get_archs(filename, platform=sys.platform):
    """
    Find the list of architectures present in a Mach-O file, or a single-element
    list on non-OS X.
    """
    architectures = []
    output = subprocess.check_output(["file", "-Lb", filename], text=True)
    for string in output.split(" "):
        if string == "arm64e":
            architectures.append("arm64e")
        elif string == "x86_64_haswell":
            architectures.append("x86_64h")
        elif string == "x86_64":
            architectures.append("x86_64")
        elif string == "i386":
            architectures.append("i386")

    return architectures


def server_has_file(filename):
    """
    Send the symbol server a HEAD request to see if it has this symbol file.
    """
    try:
        r = requests.head(
            urllib.parse.urljoin(SYMBOL_SERVER_URL, urllib.parse.quote(filename))
        )
        return r.status_code == 200
    except requests.exceptions.RequestException:
        return False


def process_file(dump_syms, path, arch, verbose, write_all):
    arch_arg = ["-a", arch]
    try:
        stderr = None if verbose else subprocess.DEVNULL
        stdout = subprocess.check_output([dump_syms] + arch_arg + [path], stderr=stderr)
    except subprocess.CalledProcessError:
        if verbose:
            print("Processing %s%s...failed." % (path, " [%s]" % arch if arch else ""))
        return None, None
    module = stdout.splitlines()[0]
    bits = module.split(b" ", 4)
    if len(bits) != 5:
        return None, None
    _, platform, cpu_arch, debug_id, debug_file = bits
    if verbose:
        sys.stdout.write("Processing %s [%s]..." % (path, arch))
    filename = os.path.join(debug_file, debug_id, debug_file + b".sym")
    # see if the server already has this symbol file
    if not write_all:
        if server_has_file(filename):
            if verbose:
                print("already on server.")
            return None, None
    # Collect for uploading
    if verbose:
        print("done.")
    return filename, stdout


def get_files(paths, platform=sys.platform):
    """
    For each entry passed in paths if the path is a file that can
    be processed, yield it, otherwise if it is a directory yield files
    under it that can be processed.
    """
    for path in paths:
        if os.path.isdir(path):
            for root, subdirs, files in os.walk(path):
                for f in files:
                    fullpath = os.path.join(root, f)
                    if should_process(fullpath, platform=platform):
                        yield fullpath
        elif should_process(path, platform=platform):
            yield path


def process_paths(
    paths, executor, dump_syms, verbose, write_all=False, platform=sys.platform
):
    jobs = set()
    for fullpath in get_files(paths, platform=platform):
        while os.path.islink(fullpath):
            fullpath = os.path.join(os.path.dirname(fullpath), os.readlink(fullpath))
        if platform == "linux":
            # See if there's a -dbg package installed and dump that instead.
            dbgpath = "/usr/lib/debug" + fullpath
            if os.path.isfile(dbgpath):
                fullpath = dbgpath
        for arch in get_archs(fullpath, platform=platform):
            jobs.add(
                executor.submit(
                    process_file, dump_syms, fullpath, arch, verbose, write_all
                )
            )
    for job in concurrent.futures.as_completed(jobs):
        try:
            yield job.result()
        except Exception as e:
            traceback.print_exc(file=sys.stderr)
            print("Error: %s" % str(e), file=sys.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Produce verbose output"
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Gather all system symbols, not just missing ones.",
    )
    parser.add_argument("dump_syms", help="Path to dump_syms binary")
    parser.add_argument(
        "files", nargs="*", help="Specific files from which to gather symbols."
    )
    args = parser.parse_args()
    args.dump_syms = os.path.abspath(args.dump_syms)
    # check for the dump_syms binary
    if (
        not os.path.isabs(args.dump_syms)
        or not os.path.exists(args.dump_syms)
        or not os.access(args.dump_syms, os.X_OK)
    ):
        print(
            "Error: can't find dump_syms binary at %s!" % args.dump_syms,
            file=sys.stderr,
        )
        return 1
    file_list = set()
    executor = concurrent.futures.ProcessPoolExecutor()
    zip_path = os.path.abspath("symbols.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for filename, contents in process_paths(
            args.files if args.files else SYSTEM_DIRS,
            executor,
            args.dump_syms,
            args.verbose,
            args.all,
        ):
            if filename and contents and filename not in file_list:
                file_list.add(filename)
                zf.writestr(filename, contents)
        zf.writestr(
            "ossyms-1.0-{platform}-{date}-symbols.txt".format(
                platform=sys.platform.title(),
                date=datetime.datetime.now().strftime("%Y%m%d%H%M%S"),
            ),
            "\n".join(file_list),
        )
    if file_list:
        if args.verbose:
            print("Generated %s with %d symbols" % (zip_path, len(file_list)))
    else:
        os.unlink("symbols.zip")


if __name__ == "__main__":
    main()
