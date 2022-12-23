#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os
import sys
from os import path
from tempfile import mkstemp

sys.path.append(path.join(path.dirname(__file__), "../python"))
logging.basicConfig(stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

from async_download import download_from_config
from mozrelease.update_verify import UpdateVerifyConfig
from util.commands import run_cmd

UPDATE_VERIFY_COMMAND = ["bash", "verify.sh", "-c"]
UPDATE_VERIFY_DIR = path.join(path.dirname(__file__), "../release/updates")


if __name__ == "__main__":
    from argparse import ArgumentParser

    parser = ArgumentParser("")

    parser.set_defaults(
        chunks=None,
        thisChunk=None,
    )
    parser.add_argument("--verify-config", required=True, dest="verifyConfig")
    parser.add_argument("--verify-channel", required=True, dest="verify_channel")
    parser.add_argument("--chunks", required=True, dest="chunks", type=int)
    parser.add_argument("--this-chunk", required=True, dest="thisChunk", type=int)
    parser.add_argument("--diff-summary", required=True, type=str)

    options = parser.parse_args()
    assert options.chunks and options.thisChunk, "chunks and this-chunk are required"
    assert path.isfile(options.verifyConfig), "Update verify config must exist!"
    verifyConfigFile = options.verifyConfig

    fd, configFile = mkstemp()
    # Needs to be opened in "bytes" mode because we perform relative seeks on it
    fh = os.fdopen(fd, "wb")
    try:
        verifyConfig = UpdateVerifyConfig()
        verifyConfig.read(path.join(UPDATE_VERIFY_DIR, verifyConfigFile))
        myVerifyConfig = verifyConfig.getChunk(options.chunks, options.thisChunk)
        # override the channel if explicitly set
        if options.verify_channel:
            myVerifyConfig.channel = options.verify_channel
        myVerifyConfig.write(fh)
        fh.close()
        run_cmd(["cat", configFile])

        # Before verifying, we want to download and cache all required files
        download_from_config(myVerifyConfig)

        run_cmd(
            UPDATE_VERIFY_COMMAND + [configFile],
            cwd=UPDATE_VERIFY_DIR,
            env={"DIFF_SUMMARY_LOG": path.abspath(options.diff_summary)},
        )
    finally:
        if path.exists(configFile):
            os.unlink(configFile)
