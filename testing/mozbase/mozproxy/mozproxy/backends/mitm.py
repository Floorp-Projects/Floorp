"""Functions to download, install, setup, and use the mitmproxy playback tool"""
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import glob
import os
import subprocess
import sys
import time
import socket

import mozinfo
from mozprocess import ProcessHandler

from mozproxy.backends.base import Playback
from mozproxy.utils import (
    transform_platform,
    tooltool_download,
    download_file_from_url,
    LOG,
)

here = os.path.dirname(__file__)
# path for mitmproxy certificate, generated auto after mitmdump is started
# on local machine it is 'HOME', however it is different on production machines
try:
    DEFAULT_CERT_PATH = os.path.join(
        os.getenv("HOME"), ".mitmproxy", "mitmproxy-ca-cert.cer"
    )
except Exception:
    DEFAULT_CERT_PATH = os.path.join(
        os.getenv("HOMEDRIVE"),
        os.getenv("HOMEPATH"),
        ".mitmproxy",
        "mitmproxy-ca-cert.cer",
    )

# On Windows, deal with mozilla-build having forward slashes in $HOME:
if os.name == "nt" and "/" in DEFAULT_CERT_PATH:
    DEFAULT_CERT_PATH = DEFAULT_CERT_PATH.replace("/", "\\")

# maximal allowed runtime of a mitmproxy command
MITMDUMP_COMMAND_TIMEOUT = 30

# to install mitmproxy certificate into Firefox and turn on/off proxy
POLICIES_CONTENT_ON = """{
  "policies": {
    "Certificates": {
      "Install": ["%(cert)s"]
    },
    "Proxy": {
      "Mode": "manual",
      "HTTPProxy": "%(host)s:8080",
      "SSLProxy": "%(host)s:8080",
      "Passthrough": "%(host)s",
      "Locked": true
    }
  }
}"""

POLICIES_CONTENT_OFF = """{
  "policies": {
    "Proxy": {
      "Mode": "none",
      "Locked": false
    }
  }
}"""


class Mitmproxy(Playback):
    def __init__(self, config):
        self.config = config
        self.mitmproxy_proc = None
        self.mitmdump_path = None
        self.browser_path = config.get("binary")
        self.policies_dir = None
        self.ignore_mitmdump_exit_failure = config.get(
            "ignore_mitmdump_exit_failure", False
        )

        if self.config.get("playback_version") is None:
            LOG.info("mitmproxy was not provided with a 'playback_version' "
                     "getting 'playback_version' from 'playback_binary_manifest'")
            if "4.0.4" in self.config["playback_binary_manifest"]:
                self.config["playback_version"] = "4.0.4"
            else:
                self.config["playback_version"] = "2.0.2"

        # mozproxy_dir is where we will download all mitmproxy required files
        # when running locally it comes from obj_path via mozharness/mach
        if self.config.get("obj_path") is not None:
            self.mozproxy_dir = self.config.get("obj_path")
        else:
            # in production it is ../tasks/task_N/build/, in production that dir
            # is not available as an envvar, however MOZ_UPLOAD_DIR is set as
            # ../tasks/task_N/build/blobber_upload_dir so take that and go up 1 level
            self.mozproxy_dir = os.path.dirname(
                os.path.dirname(os.environ["MOZ_UPLOAD_DIR"])
            )

        self.mozproxy_dir = os.path.join(self.mozproxy_dir, "testing", "mozproxy")
        self.upload_dir = os.environ.get("MOZ_UPLOAD_DIR", self.mozproxy_dir)

        LOG.info(
            "mozproxy_dir used for mitmproxy downloads and exe files: %s"
            % self.mozproxy_dir
        )
        # setting up the MOZPROXY_DIR env variable so custom scripts know
        # where to get the data
        os.environ["MOZPROXY_DIR"] = self.mozproxy_dir

    def start(self):
        # go ahead and download and setup mitmproxy
        self.download()

        # mitmproxy must be started before setup, so that the CA cert is available
        self.start_mitmproxy_playback(self.mitmdump_path, self.browser_path)

        # In case the setup fails, we want to stop the process before raising.
        try:
            self.setup()
        except Exception:
            self.stop()
            raise

    def download(self):
        """Download and unpack mitmproxy binary and pageset using tooltool"""
        if not os.path.exists(self.mozproxy_dir):
            os.makedirs(self.mozproxy_dir)

        _manifest = os.path.join(here, self.config["playback_binary_manifest"])
        transformed_manifest = transform_platform(_manifest, self.config["platform"])

        # generate the mitmdump_path
        self.mitmdump_path = os.path.join(self.mozproxy_dir, "mitmdump-%s" %
                                          self.config["playback_version"], "mitmdump")

        # Check if mitmproxy bin exists
        if os.path.exists(self.mitmdump_path):
            LOG.info("mitmproxy binary already exists. Skipping download")
        else:
            # Download and unpack mitmproxy binary
            download_path = os.path.dirname(self.mitmdump_path)
            LOG.info("create mitmproxy %s dir" % self.config["playback_version"])
            if not os.path.exists(download_path):
                os.makedirs(download_path)

            LOG.info("downloading mitmproxy binary")
            tooltool_download(
                transformed_manifest, self.config["run_local"],
                download_path)

        if "playback_pageset_manifest" in self.config:
            # we use one pageset for all platforms
            LOG.info("downloading mitmproxy pageset")
            _manifest = self.config["playback_pageset_manifest"]
            transformed_manifest = transform_platform(
                _manifest, self.config["platform"]
            )
            tooltool_download(
                transformed_manifest, self.config["run_local"], self.mozproxy_dir
            )

        if "playback_artifacts" in self.config:
            artifacts = self.config["playback_artifacts"].split(",")
            for artifact in artifacts:
                artifact = artifact.strip()
                if not artifact:
                    continue
                artifact_name = artifact.split("/")[-1]
                if artifact_name.endswith(".manifest"):
                    tooltool_download(
                        artifact, self.config["run_local"], self.mozproxy_dir
                    )
                else:
                    dest = os.path.join(self.mozproxy_dir, artifact_name)
                    download_file_from_url(artifact, dest, extract=True)

    def stop(self):
        self.stop_mitmproxy_playback()

    def start_mitmproxy_playback(self, mitmdump_path, browser_path):
        """Startup mitmproxy and replay the specified flow file"""
        if self.mitmproxy_proc is not None:
            raise Exception("Proxy already started.")
        LOG.info("mitmdump path: %s" % mitmdump_path)
        LOG.info("browser path: %s" % browser_path)

        # mitmproxy needs some DLL's that are a part of Firefox itself, so add to path
        env = os.environ.copy()
        env["PATH"] = os.path.dirname(browser_path) + os.pathsep + env["PATH"]
        command = [mitmdump_path]

        if "playback_tool_args" in self.config:
            command.extend(self.config["playback_tool_args"])

        LOG.info("Starting mitmproxy playback using env path: %s" % env["PATH"])
        LOG.info("Starting mitmproxy playback using command: %s" % " ".join(command))
        # to turn off mitmproxy log output, use these params for Popen:
        # Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        self.mitmproxy_proc = ProcessHandler(
            command, logfile=os.path.join(self.upload_dir, "mitmproxy.log"), env=env
        )
        self.mitmproxy_proc.run()
        end_time = time.time() + MITMDUMP_COMMAND_TIMEOUT
        ready = False
        while time.time() < end_time:
            ready = self.check_proxy()
            if ready:
                LOG.info(
                    "Mitmproxy playback successfully started as pid %d"
                    % self.mitmproxy_proc.pid
                )
                return
            time.sleep(0.25)
        # cannot continue as we won't be able to playback the pages
        LOG.error("Aborting: Mitmproxy process did not startup")
        self.stop_mitmproxy_playback()
        sys.exit()  # XXX why do we need to do that? a raise is not enough?

    def stop_mitmproxy_playback(self):
        """Stop the mitproxy server playback"""
        if self.mitmproxy_proc is None or self.mitmproxy_proc.poll() is not None:
            return
        LOG.info(
            "Stopping mitmproxy playback, killing process %d" % self.mitmproxy_proc.pid
        )

        exit_code = self.mitmproxy_proc.kill()
        if exit_code != 0:
            # I *think* we can still continue, as process will be automatically
            # killed anyway when mozharness is done (?) if not, we won't be able
            # to startup mitmxproy next time if it is already running
            if exit_code is None:
                LOG.error("Failed to kill the mitmproxy playback process")
            else:
                log_func = LOG.error
                if self.ignore_mitmdump_exit_failure:
                    log_func = LOG.info
                log_func("Mitmproxy exited with error code %d" % exit_code)
        else:
            LOG.info("Successfully killed the mitmproxy playback process")

        self.mitmproxy_proc = None

    def check_proxy(self, host="localhost", port=8080):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            s.connect((host, port))
            s.shutdown(socket.SHUT_RDWR)
            s.close()
            return True
        except socket.error:
            return False


class MitmproxyDesktop(Mitmproxy):
    def __init__(self, config):
        Mitmproxy.__init__(self, config)

    def setup(self):
        """
        Installs certificates.

        For Firefox we need to install the generated mitmproxy CA cert. For
        Chromium this is not necessary as it will be started with the
        --ignore-certificate-errors cmd line arg.
        """
        if not self.config["app"] == "firefox":
            return
        # install the generated CA certificate into Firefox desktop
        self.install_mitmproxy_cert(self.browser_path)

    def install_mitmproxy_cert(self, browser_path):
        """Install the CA certificate generated by mitmproxy, into Firefox
        1. Create a dir called 'distribution' in the same directory as the Firefox executable
        2. Create the policies.json file inside that folder; which points to the certificate
           location, and turns on the the browser proxy settings
        """
        LOG.info("Installing mitmproxy CA certficate into Firefox")

        # browser_path is the exe, we want the folder
        self.policies_dir = os.path.dirname(browser_path)
        # on macosx we need to remove the last folders 'MacOS'
        # and the policies json needs to go in ../Content/Resources/
        if "mac" in self.config["platform"]:
            self.policies_dir = os.path.join(self.policies_dir[:-6], "Resources")
        # for all platforms the policies json goes in a 'distribution' dir
        self.policies_dir = os.path.join(self.policies_dir, "distribution")

        self.cert_path = DEFAULT_CERT_PATH
        # for windows only
        if mozinfo.os == "win":
            self.cert_path = self.cert_path.replace("\\", "\\\\")

        if not os.path.exists(self.policies_dir):
            LOG.info("creating folder: %s" % self.policies_dir)
            os.makedirs(self.policies_dir)
        else:
            LOG.info("folder already exists: %s" % self.policies_dir)

        self.write_policies_json(
            self.policies_dir,
            policies_content=POLICIES_CONTENT_ON % {"cert": self.cert_path,
                                                    "host": self.config["host"]},
        )

        # cannot continue if failed to add CA cert to Firefox, need to check
        if not self.is_mitmproxy_cert_installed():
            LOG.error(
                "Aborting: failed to install mitmproxy CA cert into Firefox desktop"
            )
            self.stop_mitmproxy_playback()
            sys.exit()

    def write_policies_json(self, location, policies_content):
        policies_file = os.path.join(location, "policies.json")
        LOG.info("writing: %s" % policies_file)

        with open(policies_file, "w") as fd:
            fd.write(policies_content)

    def read_policies_json(self, location):
        policies_file = os.path.join(location, "policies.json")
        LOG.info("reading: %s" % policies_file)

        with open(policies_file, "r") as fd:
            return fd.read()

    def is_mitmproxy_cert_installed(self):
        """Verify mitmxproy CA cert was added to Firefox"""
        try:
            # read autoconfig file, confirm mitmproxy cert is in there
            contents = self.read_policies_json(self.policies_dir)
            LOG.info("Firefox policies file contents:")
            LOG.info(contents)
            if (
                    POLICIES_CONTENT_ON
                    % {"cert": self.cert_path, "host": self.config["host"]}
            ) in contents:
                LOG.info("Verified mitmproxy CA certificate is installed in Firefox")
            else:

                return False
        except Exception as e:
            LOG.info("failed to read Firefox policies file, exeption: %s" % e)
            return False
        return True

    def stop(self):
        self.stop_mitmproxy_playback()
        self.turn_off_browser_proxy()

    def turn_off_browser_proxy(self):
        """Turn off the browser proxy that was used for mitmproxy playback. In Firefox
        we need to change the autoconfig files to revert the proxy; for Chromium the proxy
        was setup on the cmd line, so nothing is required here."""
        if self.config["app"] == "firefox" and self.policies_dir is not None:
            LOG.info("Turning off the browser proxy")

            self.write_policies_json(
                self.policies_dir, policies_content=POLICIES_CONTENT_OFF
            )


class MitmproxyAndroid(Mitmproxy):
    def __init__(self, config, android_device):
        Mitmproxy.__init__(self, config)
        self.android_device = android_device

    @property
    def certutil_sleep_seconds(self):
        """Time to sleep, in seconds, after issuing a `certutil` command."""
        return 10 if not self.config["run_local"] else 1

    def setup(self):
        self.download_and_install_host_utils()
        self.install_mitmproxy_cert(self.browser_path)

    def download_and_install_host_utils(self):
        """
        If running locally:
        1. Will use the `certutil` tool from the local Firefox desktop build

        If running in production:
        1. Get the tooltools manifest file for downloading hostutils (contains certutil)
        2. Get the `certutil` tool by downloading hostutils using the tooltool manifest
        """
        if self.config["run_local"]:
            # when running locally, it is found in the Firefox desktop build (..obj../dist/bin)
            self.certutil = os.path.join(os.environ["MOZ_HOST_BIN"], "certutil")
            if not (
                    os.path.isfile(self.certutil) and os.access(self.certutil, os.X_OK)
            ):
                raise Exception("Abort: unable to execute certutil: {}".format(self.certutil))
            self.certutil = os.environ["MOZ_HOST_BIN"]
            os.environ["LD_LIBRARY_PATH"] = self.certutil
        else:
            # must download certutil inside hostutils via tooltool; use this manifest:
            # mozilla-central/testing/config/tooltool-manifests/linux64/hostutils.manifest
            # after it will be found here inside the worker/bitbar container:
            # /builds/worker/workspace/build/hostutils/host-utils-66.0a1.en-US.linux-x86_64
            LOG.info("downloading certutil binary (hostutils)")

            # get path to the hostutils tooltool manifest; was set earlier in
            # mozharness/configs/raptor/android_hw_config.py, to the path i.e.
            # mozilla-central/testing/config/tooltool-manifests/linux64/hostutils.manifest
            # the bitbar container is always linux64
            if os.environ.get("GECKO_HEAD_REPOSITORY", None) is None:
                raise Exception("Abort: unable to get GECKO_HEAD_REPOSITORY")

            if os.environ.get("GECKO_HEAD_REV", None) is None:
                raise Exception("Abort: unable to get GECKO_HEAD_REV")

            if os.environ.get("HOSTUTILS_MANIFEST_PATH", None) is not None:
                manifest_url = os.path.join(
                    os.environ["GECKO_HEAD_REPOSITORY"],
                    "raw-file",
                    os.environ["GECKO_HEAD_REV"],
                    os.environ["HOSTUTILS_MANIFEST_PATH"],
                )
            else:
                raise Exception("Abort: unable to get HOSTUTILS_MANIFEST_PATH!")

            # first need to download the hostutils tooltool manifest file itself
            _dest = os.path.join(self.mozproxy_dir, "hostutils.manifest")
            have_manifest = download_file_from_url(manifest_url, _dest)
            if not have_manifest:
                raise Exception("failed to download the hostutils tooltool manifest")

            # now use the manifest to download hostutils so we can get certutil
            tooltool_download(_dest, self.config["run_local"], self.mozproxy_dir)

            # the production bitbar container host is always linux
            self.certutil = glob.glob(
                os.path.join(self.mozproxy_dir, "host-utils*[!z|checksum]")
            )[0]

            # must add hostutils/certutil to the path
            os.environ["LD_LIBRARY_PATH"] = self.certutil

        bin_suffix = mozinfo.info.get("bin_suffix", "")
        self.certutil = os.path.join(self.certutil, "certutil" + bin_suffix)
        if os.path.isfile(self.certutil):
            LOG.info("certutil is found at: %s" % self.certutil)
        else:
            raise Exception("unable to find certutil at %s" % self.certutil)

    def install_mitmproxy_cert(self, browser_path):
        """Install the CA certificate generated by mitmproxy, into geckoview android

        1. Create an NSS certificate database in the geckoview browser profile dir, only
           if it doesn't already exist. Use this certutil command:
           `certutil -N -d sql:<path to profile> --empty-password`
        2. Import the mitmproxy certificate into the database, i.e.:
           `certutil -A -d sql:<path to profile> -n "some nickname" -t TC,, -a -i <path to CA.pem>`
        """

        cert_db_location = "sql:%s/" % self.config["local_profile_dir"]

        if not self.cert_db_exists(cert_db_location):
            self.create_cert_db(cert_db_location)

        # DEFAULT_CERT_PATH has local path and name of mitmproxy cert i.e.
        # /home/cltbld/.mitmproxy/mitmproxy-ca-cert.cer
        self.import_certificate_in_cert_db(cert_db_location, DEFAULT_CERT_PATH)

        # cannot continue if failed to add CA cert to Firefox, need to check
        if not self.is_mitmproxy_cert_installed(cert_db_location):
            LOG.error("Aborting: failed to install mitmproxy CA cert into Firefox")
            self.stop_mitmproxy_playback()
            sys.exit()

    def import_certificate_in_cert_db(self, cert_db_location, local_cert_path):
        # import mitmproxy cert into the db
        command = [
            self.certutil,
            "-A",
            "-d",
            cert_db_location,
            "-n",
            "mitmproxy-cert",
            "-t",
            "TC,,",
            "-a",
            "-i",
            local_cert_path,
        ]

        LOG.info(
            "importing mitmproxy cert into db using command: %s" % " ".join(command)
        )

        cmd_proc = subprocess.Popen(command, env=os.environ.copy())
        time.sleep(self.certutil_sleep_seconds)

        cmd_terminated = cmd_proc.poll()
        if cmd_terminated is None:  # None value indicates process hasn't terminated
            LOG.critical(
                "command to import mitmproxy cert into cert db failed to complete"
            )

    def create_cert_db(self, cert_db_location):
        # create cert db if it doesn't already exist; it may exist already
        # if a previous pageload test ran in the same test suite
        command = [self.certutil, "-d", cert_db_location, "-N", "--empty-password"]

        LOG.info("creating nss cert database using command: %s" % " ".join(command))
        cmd_proc = subprocess.Popen(command, env=os.environ.copy())
        time.sleep(self.certutil_sleep_seconds)
        cmd_terminated = cmd_proc.poll()
        if cmd_terminated is None:  # None value indicates process hasn't terminated
            raise Exception("nss cert db creation command failed to complete")
        if not self.cert_db_exists(cert_db_location):
            raise Exception("nss cert db creation command failed. Cert db not created.")

    def cert_db_exists(self, cert_db_location):
        # check if the nss ca cert db already exists in the device profile
        LOG.info(
            "checking if the nss cert db already exists in the android browser profile"
        )
        command = [self.certutil, "-d", cert_db_location, "-L"]

        try:
            subprocess.check_call(command, env=os.environ.copy())
            LOG.info("the nss cert db exists")
            cert_db_exists = True
        except subprocess.CalledProcessError:
            # this means the nss cert db doesn't exist yet
            LOG.info("nss cert db doesn't exist yet. Note: certutil error is expected!!!")
            cert_db_exists = False

        # try a forced pause between certutil cmds; possibly reduce later
        time.sleep(self.certutil_sleep_seconds)

        return cert_db_exists

    def is_mitmproxy_cert_installed(self, cert_db_location):
        """Verify mitmxproy CA cert was added to Firefox on android"""
        LOG.info("verifying that the mitmproxy ca cert is installed on android")

        # list the certifcates that are in the nss cert db (inside the browser profile dir)
        LOG.info(
            "getting the list of certs in the nss cert db in the android browser profile"
        )
        command = [self.certutil, "-d", cert_db_location, "-L"]

        try:
            cmd_output = subprocess.check_output(command, env=os.environ.copy())
        except subprocess.CalledProcessError:
            # cmd itself failed
            LOG.error(cmd_output)
            raise Exception("certutil command failed")

        # check output from the certutil command, see if 'mitmproxy-cert' is listed
        time.sleep(self.certutil_sleep_seconds)
        LOG.info(cmd_output)
        if "mitmproxy-cert" in cmd_output:
            LOG.info(
                "verfied the mitmproxy-cert is installed in the nss cert db on android"
            )
            return True
        return False
