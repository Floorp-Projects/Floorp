#!/usr/bin/env python

from __future__ import absolute_import, print_function

import logging
import os
from os import path
import sys
from tempfile import mkstemp

sys.path.append(path.join(path.dirname(__file__), "../python"))
logging.basicConfig(
    stream=sys.stdout, level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)

from mozrelease.update_verify import UpdateVerifyConfig
from util.commands import run_cmd

UPDATE_VERIFY_COMMAND = ["bash", "verify.sh", "-c"]
UPDATE_VERIFY_DIR = path.join(
    path.dirname(__file__), "../release/updates")


if __name__ == "__main__":
    from optparse import OptionParser
    parser = OptionParser("")

    parser.set_defaults(
        configDict="updateChannels",
        chunks=None,
        thisChunk=None,
    )
    parser.add_option("--config-dict", dest="configDict")
    parser.add_option("--verify-config", dest="verifyConfig")
    parser.add_option("-t", "--release-tag", dest="releaseTag")
    parser.add_option("-r", "--release-config", dest="releaseConfig")
    parser.add_option("-p", "--platform", dest="platform")
    parser.add_option("-C", "--release-channel", dest="release_channel")
    parser.add_option("--verify-channel", dest="verify_channel")
    parser.add_option("--chunks", dest="chunks", type="int")
    parser.add_option("--this-chunk", dest="thisChunk", type="int")

    options, args = parser.parse_args()
    assert options.chunks and options.thisChunk, \
        "chunks and this-chunk are required"
    assert path.isfile(options.verifyConfig), "Update verify config must exist!"
    verifyConfigFile = options.verifyConfig

    fd, configFile = mkstemp()
    fh = os.fdopen(fd, "w")
    try:
        verifyConfig = UpdateVerifyConfig()
        verifyConfig.read(path.join(UPDATE_VERIFY_DIR, verifyConfigFile))
        myVerifyConfig = verifyConfig.getChunk(
            options.chunks, options.thisChunk)
        # override the channel if explicitly set
        if options.verify_channel:
            myVerifyConfig.channel = options.verify_channel
        myVerifyConfig.write(fh)
        fh.close()
        run_cmd(["cat", configFile])
        run_cmd(UPDATE_VERIFY_COMMAND + [configFile], cwd=UPDATE_VERIFY_DIR)
    finally:
        if path.exists(configFile):
            os.unlink(configFile)
