#!/usr/bin/env python

# Copyright 2015 Michael R. Miller.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""
PackageSymbolDumper.py

Dumps Breakpad symbols for the contents of an Apple update installer.  Given a
path to an Apple update installer as a .dmg or a path to a specific package
within the disk image, PackageSymbolDumper mounts, traverses, and dumps symbols
for all applicable frameworks and dylibs found within.

Required tools for Linux:
    pax
    gzip
    tar
    xpwn's dmg (https://github.com/planetbeing/xpwn)

Created on Apr 11, 2012

@author: mrmiller
"""
import argparse
import concurrent.futures
import errno
import logging
import os
import shutil
import stat
import subprocess
import tempfile
import traceback

from mozpack.macpkg import Pbzx, uncpio, unxar
from scrapesymbols.gathersymbols import process_paths


def expand_pkg(pkg_path, out_path):
    """
    Expands the contents of an installer package to some directory.

    @param pkg_path: a path to an installer package (.pkg)
    @param out_path: a path to hold the package contents
    """
    for name, content in unxar(open(pkg_path, "rb")):
        with open(os.path.join(out_path, name), "wb") as fh:
            shutil.copyfileobj(content, fh)


def expand_dmg(dmg_path, out_path):
    """
    Expands the contents of a DMG file to some directory.

    @param dmg_path: a path to a disk image file (.dmg)
    @param out_path: a path to hold the image contents
    """

    with tempfile.NamedTemporaryFile() as f:
        subprocess.check_call(
            ["dmg", "extract", dmg_path, f.name], stdout=subprocess.DEVNULL
        )
        subprocess.check_call(
            ["hfsplus", f.name, "extractall"], stdout=subprocess.DEVNULL, cwd=out_path
        )


def expand_zip(zip_path, out_path):
    """
    Expands the contents of a ZIP archive to some directory.

    @param dmg_path: a path to a ZIP archive (.zip)
    @param out_path: a path to hold the archive contents
    """
    subprocess.check_call(
        ["unzip", "-d", out_path, zip_path], stdout=subprocess.DEVNULL
    )


def filter_files(function, path):
    """
    Yield file paths matching a filter function by walking the
    hierarchy rooted at path.

    @param function: a function taking in a filename that returns true to
        include the path
    @param path: the root path of the hierarchy to traverse
    """
    for root, _dirs, files in os.walk(path):
        for filename in files:
            if function(filename):
                yield os.path.join(root, filename)


def find_packages(path):
    """
    Returns a list of installer packages (as determined by the .pkg extension),
    disk images (as determined by the .dmg extension) or ZIP archives found
    within path.

    @param path: root path to search for .pkg, .dmg and .zip files
    """
    return filter_files(
        lambda filename: os.path.splitext(filename)[1] in (".pkg", ".dmg", ".zip")
        and not filename.startswith("._"),
        path,
    )


def find_all_packages(paths):
    """
    Yield installer package files, disk images and ZIP archives found in all
    of `paths`.

    @param path: list of root paths to search for .pkg & .dmg files
    """
    for path in paths:
        logging.info("find_all_packages: {}".format(path))
        for pkg in find_packages(path):
            yield pkg


def find_payloads(path):
    """
    Returns a list of possible installer package payload paths.

    @param path: root path for an installer package
    """
    return filter_files(
        lambda filename: "Payload" in filename or ".pax.gz" in filename, path
    )


def extract_payload(payload_path, output_path):
    """
    Extracts the contents of an installer package payload to a given directory.

    @param payload_path: path to an installer package's payload
    @param output_path: output path for the payload's contents
    @return True for success, False for failure.
    """
    header = open(payload_path, "rb").read(2)
    try:
        if header == b"BZ":
            logging.info("Extracting bzip2 payload")
            extract = "bzip2"
            subprocess.check_call(
                'cd {dest} && {extract} -dc {payload} | pax -r -k -s ":^/::"'.format(
                    extract=extract, payload=payload_path, dest=output_path
                ),
                shell=True,
            )
            return True
        elif header == b"\x1f\x8b":
            logging.info("Extracting gzip payload")
            extract = "gzip"
            subprocess.check_call(
                'cd {dest} && {extract} -dc {payload} | pax -r -k -s ":^/::"'.format(
                    extract=extract, payload=payload_path, dest=output_path
                ),
                shell=True,
            )
            return True
        elif header == b"pb":
            logging.info("Extracting pbzx payload")

            for path, st, content in uncpio(Pbzx(open(payload_path, "rb"))):
                if not path or not stat.S_ISREG(st.mode):
                    continue
                out = os.path.join(output_path, path.decode())
                os.makedirs(os.path.dirname(out), exist_ok=True)
                with open(out, "wb") as fh:
                    shutil.copyfileobj(content, fh)

            return True
        else:
            # Unsupported format
            logging.error(
                "Unknown payload format: 0x{0:x}{1:x}".format(header[0], header[1])
            )
            return False

    except Exception:
        return False


def shutil_error_handler(caller, path, excinfo):
    logging.error('Could not remove "{path}": {info}'.format(path=path, info=excinfo))


def write_symbol_file(dest, filename, contents):
    full_path = os.path.join(dest, filename)
    try:
        os.makedirs(os.path.dirname(full_path))
        with open(full_path, "wb") as sym_file:
            sym_file.write(contents)
    except os.error as e:
        if e.errno != errno.EEXIST:
            raise


def dump_symbols(executor, dump_syms, path, dest):
    system_library = os.path.join("System", "Library")
    subdirectories = [
        os.path.join(system_library, "Frameworks"),
        os.path.join(system_library, "PrivateFrameworks"),
        os.path.join(system_library, "Extensions"),
        os.path.join("usr", "lib"),
    ]

    paths_to_dump = [os.path.join(path, d) for d in subdirectories]
    existing_paths = [path for path in paths_to_dump if os.path.exists(path)]

    for filename, contents in process_paths(
        paths=existing_paths,
        executor=executor,
        dump_syms=dump_syms,
        verbose=True,
        write_all=True,
        platform="darwin",
    ):
        if filename and contents:
            logging.info("Added symbol file " + str(filename, "utf-8"))
            write_symbol_file(dest, str(filename, "utf-8"), contents)


def dump_symbols_from_payload(executor, dump_syms, payload_path, dest):
    """
    Dumps all the symbols found inside the payload of an installer package.

    @param dump_syms: path to the dump_syms executable
    @param payload_path: path to an installer package's payload
    @param dest: output path for symbols
    """
    temp_dir = None
    logging.info("Dumping symbols from payload: " + payload_path)
    try:
        temp_dir = tempfile.mkdtemp()
        logging.info("Extracting payload to {path}.".format(path=temp_dir))
        if not extract_payload(payload_path, temp_dir):
            logging.error("Could not extract payload: " + payload_path)
            return False

        dump_symbols(executor, dump_syms, temp_dir, dest)

    finally:
        if temp_dir is not None:
            shutil.rmtree(temp_dir, onerror=shutil_error_handler)

    return True


def dump_symbols_from_package(executor, dump_syms, pkg, dest):
    """
    Dumps all the symbols found inside an installer package.

    @param dump_syms: path to the dump_syms executable
    @param pkg: path to an installer package
    @param dest: output path for symbols
    """
    successful = True
    temp_dir = None
    logging.info("Dumping symbols from package: " + pkg)
    try:
        temp_dir = tempfile.mkdtemp()
        if os.path.splitext(pkg)[1] == ".pkg":
            expand_pkg(pkg, temp_dir)
        elif os.path.splitext(pkg)[1] == ".zip":
            expand_zip(pkg, temp_dir)
        else:
            expand_dmg(pkg, temp_dir)

        # check for any subpackages
        for subpackage in find_packages(temp_dir):
            logging.info("Found subpackage at: " + subpackage)
            res = dump_symbols_from_package(executor, dump_syms, subpackage, dest)
            if not res:
                logging.error("Error while dumping subpackage: " + subpackage)

        # dump symbols from any payloads (only expecting one) in the package
        for payload in find_payloads(temp_dir):
            res = dump_symbols_from_payload(executor, dump_syms, payload, dest)
            if not res:
                successful = False

        # dump symbols directly extracted from the package
        dump_symbols(executor, dump_syms, temp_dir, dest)

    except Exception as e:
        traceback.print_exc()
        logging.error("Exception while dumping symbols from package: {}".format(e))
        successful = False

    finally:
        if temp_dir is not None:
            shutil.rmtree(temp_dir, onerror=shutil_error_handler)

    return successful


def read_processed_packages(tracking_file):
    if tracking_file is None or not os.path.exists(tracking_file):
        return set()
    logging.info("Reading processed packages from {}".format(tracking_file))
    return set(open(tracking_file, "r").read().splitlines())


def write_processed_packages(tracking_file, processed_packages):
    if tracking_file is None:
        return
    logging.info(
        "Writing {} processed packages to {}".format(
            len(processed_packages), tracking_file
        )
    )
    open(tracking_file, "w").write("\n".join(processed_packages))


def process_packages(package_finder, to, tracking_file, dump_syms):
    processed_packages = read_processed_packages(tracking_file)
    with concurrent.futures.ProcessPoolExecutor() as executor:
        for pkg in package_finder():
            if pkg in processed_packages:
                logging.info("Skipping already-processed package: {}".format(pkg))
            else:
                dump_symbols_from_package(executor, dump_syms, pkg, to)
                processed_packages.add(pkg)
                write_processed_packages(tracking_file, processed_packages)


def main():
    parser = argparse.ArgumentParser(
        description="Extracts Breakpad symbols from a Mac OS X support update."
    )
    parser.add_argument(
        "--dump_syms",
        default="dump_syms",
        type=str,
        help="path to the Breakpad dump_syms executable",
    )
    parser.add_argument(
        "--tracking-file",
        type=str,
        help="Path to a file in which to store information "
        + "about already-processed packages",
    )
    parser.add_argument(
        "search", nargs="+", help="Paths to search recursively for packages"
    )
    parser.add_argument("to", type=str, help="destination path for the symbols")
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    )
    for p in ("requests.packages.urllib3.connectionpool", "urllib3"):
        urllib3_logger = logging.getLogger(p)
        urllib3_logger.setLevel(logging.ERROR)

    if not args.search or not all(os.path.exists(p) for p in args.search):
        logging.error("Invalid search path")
        return
    if not os.path.exists(args.to):
        logging.error("Invalid path to destination")
        return

    def finder():
        return find_all_packages(args.search)

    process_packages(finder, args.to, args.tracking_file, args.dump_syms)


if __name__ == "__main__":
    main()
