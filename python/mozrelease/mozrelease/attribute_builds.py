#! /usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import json
import logging
import mmap
import os
import shutil
import struct
import sys
import tempfile
import urllib.parse
from pathlib import Path

logging.basicConfig(level=logging.INFO, format="%(levelname)s - %(message)s")
log = logging.getLogger()


def write_attribution_data(filepath, data):
    """Insert data into a prepared certificate in a signed PE file.

    Returns False if the file isn't a valid PE file, or if the necessary
    certificate was not found.

    This function assumes that somewhere in the given file's certificate table
    there exists a 1024-byte space which begins with the tag "__MOZCUSTOM__:".
    The given data will be inserted into the file following this tag.

    We don't bother updating the optional header checksum.
    Windows doesn't check it for executables, only drivers and certain DLL's.
    """
    with open(filepath, "r+b") as file:
        mapped = mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_WRITE)

        # Get the location of the PE header and the optional header
        pe_header_offset = struct.unpack("<I", mapped[0x3C:0x40])[0]
        optional_header_offset = pe_header_offset + 24

        # Look up the magic number in the optional header,
        # so we know if we have a 32 or 64-bit executable.
        # We need to know that so that we can find the data directories.
        pe_magic_number = struct.unpack(
            "<H", mapped[optional_header_offset : optional_header_offset + 2]
        )[0]
        if pe_magic_number == 0x10B:
            # 32-bit
            cert_dir_entry_offset = optional_header_offset + 128
        elif pe_magic_number == 0x20B:
            # 64-bit. Certain header fields are wider.
            cert_dir_entry_offset = optional_header_offset + 144
        else:
            # Not any known PE format
            mapped.close()
            return False

        # The certificate table offset and length give us the valid range
        # to search through for where we should put our data.
        cert_table_offset = struct.unpack(
            "<I", mapped[cert_dir_entry_offset : cert_dir_entry_offset + 4]
        )[0]
        cert_table_size = struct.unpack(
            "<I", mapped[cert_dir_entry_offset + 4 : cert_dir_entry_offset + 8]
        )[0]

        if cert_table_offset == 0 or cert_table_size == 0:
            # The file isn't signed
            mapped.close()
            return False

        tag = b"__MOZCUSTOM__:"
        tag_index = mapped.find(
            tag, cert_table_offset, cert_table_offset + cert_table_size
        )
        if tag_index == -1:
            mapped.close()
            return False

        # convert to quoted-url byte-string for insertion
        data = urllib.parse.quote(data).encode("utf-8")
        mapped[tag_index + len(tag) : tag_index + len(tag) + len(data)] = data

        return True


def validate_attribution_code(attribution):
    log.info("Checking attribution %s" % attribution)
    return_code = True

    if len(attribution) == 0:
        log.error("Attribution code has 0 length")
        return False

    # Set to match https://searchfox.org/mozilla-central/rev/a92ed79b0bc746159fc31af1586adbfa9e45e264/browser/components/attribution/AttributionCode.jsm#24  # noqa
    MAX_LENGTH = 1010
    if len(attribution) > MAX_LENGTH:
        log.error("Attribution code longer than %s chars" % MAX_LENGTH)
        return_code = False

    # this leaves out empty values like 'foo='
    params = urllib.parse.parse_qsl(attribution)
    used_keys = set()
    for key, value in params:
        # check for invalid keys
        if key not in (
            "source",
            "medium",
            "campaign",
            "content",
            "experiment",
            "variation",
            "ua",
            "dlsource",
        ):
            log.error("Invalid key %s" % key)
            return_code = False

        # avoid ambiguity from repeated keys
        if key in used_keys:
            log.error("Repeated key %s" % key)
            return_code = False
        else:
            used_keys.add(key)

        # TODO the service checks for valid source, should we do that here too ?

    # We have two types of attribution with different requirements:
    # 1) Partner attribution, which requires a few UTM parameters sets
    # 2) Attribution of vanilla builds, which only requires `dlsource`
    #
    # Perhaps in an ideal world we would check what type of build we're
    # attributing to make sure that eg: partner builds don't get `dlsource`
    # instead of what they actually want -- but the likelyhood of that
    # happening is vanishingly small, so it's probably not worth doing.
    if "dlsource" not in used_keys:
        for key in ("source", "medium", "campaign", "content"):
            if key not in used_keys:
                return_code = False

    if return_code is False:
        log.error(
            "Either 'dlsource' must be provided, or all of: 'source', 'medium', 'campaign', and 'content'. Use '(not set)' if one of the latter is not needed."
        )
    return return_code


def main():
    parser = argparse.ArgumentParser(
        description="Add attribution to Windows installer(s).",
        epilog="""
        By default, configuration from envvar ATTRIBUTION_CONFIG is used, with
        expected format
          [{"input": "in/abc.exe", "output": "out/def.exe", "attribution": "abcdef"},
           {"input": "in/ghi.exe", "output": "out/jkl.exe", "attribution": "ghijkl"}]
        for 1 or more attributions. Or the script arguments may be used for a single attribution.

        The attribution code should be a string which is not url-encoded.

        If command line arguments are used instead, one or more `--input` parameters may be provided.
        Each will be written to the `--output` directory provided to a file of the same name as the
        input filename. All inputs will be attributed with the same `--attribution` code.
        """,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--input",
        default=[],
        action="append",
        help="Source installer to attribute; may be specified multiple times",
    )
    parser.add_argument("--output", help="Location to write the attributed installers")
    parser.add_argument("--attribution", help="Attribution code")
    args = parser.parse_args()

    if os.environ.get("ATTRIBUTION_CONFIG"):
        work = json.loads(os.environ["ATTRIBUTION_CONFIG"])
    elif args.input and args.output and args.attribution:
        work = []
        for i in args.input:
            fn = os.path.basename(i)
            work.append(
                {
                    "input": i,
                    "output": os.path.join(args.output, fn),
                    "attribution": args.attribution,
                }
            )
    else:
        log.error("No configuration found. Set ATTRIBUTION_CONFIG or pass arguments.")
        return 1

    cached_code_checks = []
    for job in work:
        if job["attribution"] not in cached_code_checks:
            status = validate_attribution_code(job["attribution"])
            if status:
                cached_code_checks.append(job["attribution"])
            else:
                log.error("Failed attribution code check")
                return 1

        with tempfile.TemporaryDirectory() as td:
            log.info("Attributing installer %s ..." % job["input"])
            tf = shutil.copy(job["input"], td)
            if write_attribution_data(tf, job["attribution"]):
                Path(job["output"]).parent.mkdir(parents=True, exist_ok=True)
                shutil.move(tf, job["output"])
                log.info("Wrote %s" % job["output"])


if __name__ == "__main__":
    sys.exit(main())
