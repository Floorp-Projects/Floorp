# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This module needs to stay Python 2 and 3 compatible
#
from __future__ import absolute_import
from __future__ import print_function

import platform
import time
import os
import shutil
import contextlib
import yaml
from subprocess import Popen, PIPE
import traceback
import sys
import tempfile
from datetime import datetime

import requests
from requests.exceptions import ConnectionError

from condprof import progress


TASK_CLUSTER = "TASKCLUSTER_WORKER_TYPE" in os.environ.keys()


class ArchiveNotFound(Exception):
    pass


DEFAULT_PREFS = {
    "focusmanager.testmode": True,
    "marionette.defaultPrefs.port": 2828,
    "marionette.port": 2828,
    "marionette.enabled": True,
    "marionette.log.level": "Trace",
    "marionette.log.truncate": False,
    "marionette.contentListener": False,
    "extensions.autoDisableScopes": 10,
    "devtools.debugger.remote-enabled": True,
    "devtools.console.stdout.content": True,
    "devtools.console.stdout.chrome": True,
}

DEFAULT_CUSTOMIZATION = os.path.join(
    os.path.dirname(__file__), "customization", "default.json"
)

_LOGGER = None


class NullLogger:
    def info(self, *args, **kw):
        # XXX only if debug
        # print("%s %s" % (str(args), str(kw)))
        pass

    def visit_url(self, index, total, url):
        print("%d/%d %s" % (index, total, url))

    def msg(self, event):
        print(event)

    def error(self, event, *args, **kw):
        print(event)
        traceback.print_exc(file=sys.stdout)


def get_logger():
    global _LOGGER
    if _LOGGER is not None:
        return _LOGGER

    if sys.version_info.major == 3:
        # plugging the logger into arsenic
        try:
            from arsenic import connection
            from structlog import wrap_logger

            logger = wrap_logger(NullLogger(), processors=[])
            connection.log = logger
        except ImportError:
            logger = NullLogger()
    else:
        # on python 2, just using the plain logger
        logger = NullLogger()

    _LOGGER = logger
    return _LOGGER


def LOG(msg):
    msg = "[%s] %s" % (datetime.now().isoformat(), msg)
    get_logger().msg(msg)


def ERROR(msg):
    msg = "[%s] %s" % (datetime.now().isoformat(), msg)
    get_logger().error(msg)


def fresh_profile(profile, customization_data):
    from mozprofile import create_profile  # NOQA

    # XXX on android we mgiht need to run it on the device?
    LOG("Creating a fresh profile")
    new_profile = create_profile(app="firefox")
    prefs = customization_data["prefs"]
    prefs.update(DEFAULT_PREFS)
    new_profile.set_preferences(prefs)
    extensions = []
    for name, url in customization_data["addons"].items():
        LOG("Downloading addon %s" % name)
        extension = download_file(url)
        extensions.append(extension)
    new_profile.addons.install(extensions)
    shutil.copytree(new_profile.profile, profile)
    return profile


link = "https://ftp.mozilla.org/pub/firefox/nightly/latest-mozilla-central/"


def get_firefox_download_link():
    try:
        from bs4 import BeautifulSoup
    except ImportError:
        raise ImportError("You need to run pip install beautifulsoup4")
    if platform.system() == "Darwin":
        extension = ".dmg"
    elif platform.system() == "Linux":
        arch = platform.machine()
        extension = ".linux-%s.tar.bz2" % arch
    else:
        raise NotImplementedError(platform.system())

    page = requests.get(link).text
    soup = BeautifulSoup(page, "html.parser")
    for node in soup.find_all("a", href=True):
        href = node["href"]
        if href.endswith(extension):
            return "https://ftp.mozilla.org" + href

    raise Exception()


def check_exists(archive, server=None):
    if server is not None:
        archive = server + "/" + archive
    try:
        resp = requests.head(archive)
    except ConnectionError:
        return False, {}

    if resp.status_code in (302, 303):
        return check_exists(resp.headers["Location"])

    # see Bug 1574854
    if resp.status_code == 200 and "text/html" in resp.headers["Content-Type"]:
        exists = False
    else:
        exists = resp.status_code

    return exists, resp.headers


def download_file(url, target=None):
    present, headers = check_exists(url)
    if not present:
        LOG("Cannot find %r" % url)
        raise ArchiveNotFound(url)

    etag = headers.get("ETag")
    if target is None:
        target = url.split("/")[-1]

    if os.path.exists(target):
        # XXX for now, reusing downloads without checking them
        # when we don't have an .etag file
        if etag is None or not os.path.exists(target + ".etag"):
            return target
        with open(target + ".etag") as f:
            current_etag = f.read()
        if etag == current_etag:
            LOG("Already Downloaded.")
            # should at least check the size?
            return target
        else:
            LOG("Changed!")

    LOG("Downloading %s" % url)
    req = requests.get(url, stream=True)
    total_length = int(req.headers.get("content-length"))
    target_dir = os.path.dirname(target)
    if target_dir != "" and not os.path.exists(target_dir):
        os.makedirs(target_dir)
    with open(target, "wb") as f:
        if TASK_CLUSTER:
            for chunk in req.iter_content(chunk_size=1024):
                if chunk:
                    f.write(chunk)
                    f.flush()
        else:
            iter = req.iter_content(chunk_size=1024)
            size = total_length / 1024 + 1
            for chunk in progress.bar(iter, expected_size=size):
                if chunk:
                    f.write(chunk)
                    f.flush()

    if etag is not None:
        with open(target + ".etag", "w") as f:
            f.write(etag)

    return target


def extract_from_dmg(dmg, target):
    mount = tempfile.mkdtemp()
    cmd = "hdiutil attach -nobrowse -mountpoint %s %s"
    os.system(cmd % (mount, dmg))
    try:
        found = False
        for f in os.listdir(mount):
            if not f.endswith(".app"):
                continue
            app = os.path.join(mount, f)
            shutil.copytree(app, target)
            found = True
            break
    finally:
        os.system("hdiutil detach " + mount)
        shutil.rmtree(mount)
    if not found:
        raise IOError("No app file found in %s" % dmg)


@contextlib.contextmanager
def latest_nightly(binary=None):

    if binary is None:
        # we want to use the latest nightly
        nightly_archive = get_firefox_download_link()
        LOG("Downloading %s" % nightly_archive)
        target = download_file(nightly_archive)
        # on macOs we just mount the DMG
        # XXX replace with extract_from_dmg
        if platform.system() == "Darwin":
            cmd = "hdiutil attach -mountpoint /Volumes/Nightly %s"
            os.system(cmd % target)
            binary = "/Volumes/Nightly/Firefox Nightly.app/Contents/MacOS/firefox"
        # on linux we unpack it
        elif platform.system() == "Linux":
            cmd = "bunzip2 %s" % target
            os.system(cmd)
            cmd = "tar -xvf %s" % target[: -len(".bz2")]
            os.system(cmd)
            binary = "firefox/firefox"

        mounted = True
    else:
        mounted = False
    try:
        yield binary
    finally:
        # XXX replace with extract_from_dmg
        if mounted:
            if platform.system() == "Darwin":
                LOG("Unmounting Firefox")
                time.sleep(10)
                os.system("hdiutil detach /Volumes/Nightly")
            elif platform.system() == "Linux":
                # XXX we should keep it for next time
                shutil.rmtree("firefox")


def write_yml_file(yml_file, yml_data):
    get_logger().info("writing %s to %s" % (yml_data, yml_file))
    try:
        with open(yml_file, "w") as outfile:
            yaml.dump(yml_data, outfile, default_flow_style=False)
    except Exception as e:
        get_logger().critical("failed to write yaml file, exeption: %s" % e)


def get_version(firefox):
    p = Popen([firefox, "--version"], stdin=PIPE, stdout=PIPE, stderr=PIPE)
    output, __ = p.communicate()
    res = output.strip().split()[-1]
    return res.decode("utf-8")


def get_current_platform():
    """Returns a combination of system and arch info that matches TC standards.

    e.g. macosx64, win32, linux64, etc..
    """
    arch = sys.maxsize == 2 ** 63 - 1 and "64" or "32"
    plat = platform.system().lower()
    if plat == "windows":
        plat = "win"
    elif plat == "darwin":
        plat = "macosx"
    return plat + arch


class BaseEnv:
    def __init__(self, profile, firefox, geckodriver, archive, device_name):
        self.profile = profile
        self.firefox = firefox
        self.geckodriver = geckodriver
        if profile is None:
            self.profile = os.path.join(tempfile.mkdtemp(), "profile")
        else:
            self.profile = profile
        self.archive = archive
        self.device_name = device_name

    @property
    def target_platform(self):
        return self.get_target_platform()

    def get_target_platform(self):
        raise NotImplementedError()

    def get_device(self, *args, **kw):
        raise NotImplementedError()

    @contextlib.contextmanager
    def get_browser(self, path):
        raise NotImplementedError()

    def get_browser_args(self, headless):
        raise NotImplementedError()

    def prepare(self, logfile):
        pass

    def check_session(self, session):
        pass

    def dump_logs(self):
        pass

    def get_browser_version(self):
        raise NotImplementedError()

    def get_geckodriver(self, log_file):
        raise NotImplementedError()

    def collect_profile(self):
        pass

    def stop_browser(self):
        pass
