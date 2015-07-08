#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import sys, os, re, multiprocessing
import subprocess, pprint, time, datetime, getpass, platform
from ftplib import FTP

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript
from mozharness.mozilla.purge import PurgeMixin

CONFIG_INI = {
    "avd.ini.encoding": "ISO-8859-1",
    "hw.dPad": "no",
    "hw.lcd.density": "160",
    "sdcard.size": "500M",
    "hw.cpu.arch": "arm",
    "hw.gpu.enabled": "yes",
    "hw.device.hash": "-671137758",
    "hw.camera.back": "none",
    "disk.dataPartition.size": "600M",
    "skin.path": "1024x816",
    "skin.dynamic": "yes",
    "hw.keyboard.lid": "yes",
    "hw.keyboard": "yes",
    "hw.ramSize": "1024",
    "hw.device.manufacturer": "User",
    "hw.sdCard": "yes",
    "hw.mainKeys": "no",
    "hw.accelerometer": "yes",
    "skin.name": "1024x816",
    "hw.trackBall": "no",
    "hw.device.name": "mozilla-device",
    "hw.battery": "yes",
    "hw.sensors.proximity": "yes",
    "hw.sensors.orientation": "yes",
    "hw.audioInput": "yes",
    "hw.gps": "yes",
    "vm.heapSize": "64",
    }


MOZILLA_DEVICE_DEFINITION = """
<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<d:devices xmlns:d="http://schemas.android.com/sdk/devices/1" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <d:device>
    <d:name>mozilla-device</d:name>
    <d:manufacturer>User</d:manufacturer>
    <d:meta/>
    <d:hardware>
      <d:screen>
        <d:screen-size>large</d:screen-size>
        <d:diagonal-length>7.00</d:diagonal-length>
        <d:pixel-density>mdpi</d:pixel-density>
        <d:screen-ratio>long</d:screen-ratio>
        <d:dimensions>
          <d:x-dimension>1024</d:x-dimension>
          <d:y-dimension>816</d:y-dimension>
        </d:dimensions>
        <d:xdpi>187.05</d:xdpi>
        <d:ydpi>187.05</d:ydpi>
        <d:touch>
          <d:multitouch>jazz-hands</d:multitouch>
          <d:mechanism>finger</d:mechanism>
          <d:screen-type>capacitive</d:screen-type>
        </d:touch>
      </d:screen>
      <d:networking>
Bluetooth
Wifi
NFC</d:networking>
      <d:sensors>
Accelerometer
Barometer
Compass
GPS
Gyroscope
LightSensor
ProximitySensor</d:sensors>
      <d:mic>true</d:mic>
      <d:camera>
        <d:location>back</d:location>
        <d:autofocus>true</d:autofocus>
        <d:flash>true</d:flash>
      </d:camera>
      <d:keyboard>qwerty</d:keyboard>
      <d:nav>nonav</d:nav>
      <d:ram unit="GiB">1</d:ram>
      <d:buttons>soft</d:buttons>
      <d:internal-storage unit="GiB">
4</d:internal-storage>
      <d:removable-storage unit="TiB"/>
      <d:cpu>Generic CPU</d:cpu>
      <d:gpu>Generic GPU</d:gpu>
      <d:abi>
armeabi
armeabi-v7a
x86
mips</d:abi>
      <d:dock/>
      <d:power-type>battery</d:power-type>
    </d:hardware>
    <d:software>
      <d:api-level>-</d:api-level>
      <d:live-wallpaper-support>true</d:live-wallpaper-support>
      <d:bluetooth-profiles/>
      <d:gl-version>2.0</d:gl-version>
      <d:gl-extensions/>
      <d:status-bar>false</d:status-bar>
    </d:software>
    <d:state default="true" name="Portrait">
      <d:description>The device in portrait orientation</d:description>
      <d:screen-orientation>port</d:screen-orientation>
      <d:keyboard-state>keyshidden</d:keyboard-state>
      <d:nav-state>navhidden</d:nav-state>
    </d:state>
    <d:state name="Landscape">
      <d:description>The device in landscape orientation</d:description>
      <d:screen-orientation>land</d:screen-orientation>
      <d:keyboard-state>keyshidden</d:keyboard-state>
      <d:nav-state>navhidden</d:nav-state>
    </d:state>
    <d:state name="Portrait with keyboard">
      <d:description>The device in portrait orientation with a keyboard open</d:description>
      <d:screen-orientation>land</d:screen-orientation>
      <d:keyboard-state>keysexposed</d:keyboard-state>
      <d:nav-state>navhidden</d:nav-state>
    </d:state>
    <d:state name="Landscape with keyboard">
      <d:description>The device in landscape orientation with a keyboard open</d:description>
      <d:screen-orientation>land</d:screen-orientation>
      <d:keyboard-state>keysexposed</d:keyboard-state>
      <d:nav-state>navhidden</d:nav-state>
    </d:state>
  </d:device>
</d:devices>
"""

def sniff_host_arch():
    host_arch = 'x86_64'
    if platform.architecture()[0] == '32bit':
        host_arch = 'x86'
    return host_arch

class EmulatorBuild(BaseScript, PurgeMixin):

    config_options = [
        [["--host-arch"], {
            "dest": "host_arch",
            "help": "architecture of the host the emulator will run on (x86, x86_64; default autodetected)",
        }],
        [["--target-arch"], {
            "dest": "target_arch",
            "help": "architecture of the target the emulator will emulate (armv5te, armv7a, x86; default armv7a)",
        }],
        [["--android-version"], {
            "dest": "android_version",
            "help": "android version to build (eg. 2.3.7, 4.0, 4.3.1, gingerbread, ics, jb; default gingerbread)",
        }],
        [["--android-tag"], {
            "dest": "android_tag",
            "help": "android tag to check out (eg. android-2.3.7_r1; default inferred from --android-version)",
        }],
        [["--patch"], {
            "dest": "patch",
            "help": "'dir=url' comma-separated list of patches to apply to AOSP before building (eg. development=http://foo.com/bar.patch; default inferred)",
        }],
        [["--android-apilevel"], {
            "dest": "android_apilevel",
            "help": "android API-level to build AVD for (eg. 10, 14, 18; default inferred from --android-version)",
        }],
        [["--android-url"], {
            "dest": "android_url",
            "help": "where to fetch AOSP from, default https://android.googlesource.com/platform/manifest",
        }],
        [["--ndk-version"], {
            "dest": "ndk_version",
            "help": "version of the NDK to fetch, default r9",
        }],
        [["--install-android-dir"], {
            "dest": "install_android_dir",
            "help": "location bundled AVDs will be unpacked to, default /home/cltbld/.android"
        }],
        [["--avd-count"], {
            "dest": "avd_count",
            "help": "number of AVDs to build, default 4"
        }],
        [["--jdk"], {
            "dest": "jdk",
            "help": "which jdk to use (sun or openjdk; default sun)"
        }]
    ]

    def __init__(self, require_config_file=False):

        BaseScript.__init__(self,
                            config_options=self.config_options,
                            all_actions=[
                                'clobber',
                                'apt-get-dependencies',
                                'download-aosp',
                                'download-kernel',
                                'download-ndk',
                                'download-test-binaries',
                                'checkout-orangutan',
                                'patch-aosp',
                                'build-aosp',
                                'build-kernel',
                                'build-orangutan-su',
                                'make-base-avd',
                                'customize-avd',
                                'clone-customized-avd',
                                'bundle-avds',
                                'bundle-emulators'
                            ],
                            default_actions=[
                                'apt-get-dependencies',
                                'download-aosp',
                                'download-kernel',
                                'download-ndk',
                                'download-test-binaries',
                                'checkout-orangutan',
                                'patch-aosp',
                                'build-aosp',
                                'build-kernel',
                                'build-orangutan-su',
                                'make-base-avd',
                                'customize-avd',
                                'clone-customized-avd',
                                'bundle-avds',
                                'bundle-emulators'
                            ],
                            require_config_file=require_config_file,
                            # Default configuration
                            config={
                                'host_arch': sniff_host_arch(),
                                'target_arch': 'armv7a',
                                'android_version': 'gingerbread',
                                'android_tag': 'inferred',
                                'patch': 'inferred',
                                'android_apilevel': 'inferred',
                                'work_dir': 'android_emulator_build',
                                'android_url': 'https://android.googlesource.com/platform/manifest',
                                'ndk_version': 'r9',
                                'install_android_dir': '/home/cltbld/.android',
                                'avd_count': '4',
                                'jdk': 'sun'
                            })

        if platform.system() != "Linux":
            self.fatal("this script only works on (ubuntu) linux")

        if platform.dist() != ('Ubuntu', '12.04', 'precise'):
            self.fatal("this script only works on ubuntu 12.04 precise")

        if not (platform.machine() in ['i386', 'i486', 'i586', 'i686', 'x86_64']):
            self.fatal("this script only works on x86 and x86_64")

        self.tag = self.config['android_tag']
        if self.tag == 'inferred':
            self.tag = self.select_android_tag(self.config['android_version'])

        self.patches = self.config['patch']
        if self.patches == 'inferred':
            self.patches = self.select_patches(self.tag)
        else:
            self.patches = [x.split('=') for x in self.patches.split(',')]

        self.apilevel = self.config['android_apilevel']
        if self.apilevel == 'inferred':
            self.apilevel = self.android_apilevel(self.tag)

        self.workdir = os.path.abspath(self.config['work_dir'])
        self.bindir = os.path.join(self.workdir, "bin")
        self.aospdir = os.path.join(self.workdir, "aosp")
        self.goldfishdir = os.path.join(self.workdir, "goldfish")
        self.ndkdir = os.path.join(self.workdir, "android-ndk-" + self.config['ndk_version'])
        self.ncores = multiprocessing.cpu_count()
        self.androiddir = os.path.join(self.workdir, ".android")
        self.avddir = os.path.join(self.androiddir, "avd")
        self.aosphostdir = os.path.join(self.aospdir, "out/host/linux-x86")
        self.aosphostbindir = os.path.join(self.aosphostdir, "bin")
        self.aospprodoutdir = os.path.join(self.aospdir, "out/target/product/generic")
        self.emu = os.path.join(self.aosphostbindir, "emulator")
        self.adb = os.path.join(self.aosphostbindir, "adb")
        self.navds = int(self.config['avd_count'])

    def apt_get_dependencies(self):
        jdk = "oracle-java6-installer"
        if self.config['jdk'] == "openjdk":
            jdk = "openjdk-6-jdk"
        self.apt_update()
        self.apt_get(["python-software-properties"])
        for repo in [ "ppa:webupd8team/java",
                      "deb http://archive.canonical.com/ precise partner",
                      "deb http://us.archive.ubuntu.com/ubuntu/ precise universe",
                      "deb http://us.archive.ubuntu.com/ubuntu/ precise-updates universe",
                      "deb http://us.archive.ubuntu.com/ubuntu/ precise-backports main restricted universe multiverse",
                      "deb http://security.ubuntu.com/ubuntu precise-security universe" ]:
            self.apt_add_repo(repo)
        self.apt_update()
        self.apt_get(["python-software-properties"])
        for pkgset in [[jdk],
                       ["libglw1-mesa"],
                       ["git", "gnupg", "flex", "bison", "gperf", "zip", "curl"],
                       ["mingw32", "tofrodos"],
                       ["build-essential", "gcc-4.4-multilib", "g++-4.4-multilib"],
                       ["python-markdown", "libxml2-utils", "xsltproc"],
                       ["x11proto-core-dev:i386", "libx11-dev:i386", "libreadline6-dev:i386",
                        "libgl1-mesa-glx:i386", "libgl1-mesa-dev:i386", "mesa-common-dev:i386",
                        "libxext-dev:i386"],
                       ["libc6-dev:i386", "libncurses5-dev:i386", "zlib1g-dev:i386"],
                       ["zlib1g-dev"],
                       ["gcc", "g++"]]:
            self.apt_get(pkgset)


    def download_aosp(self):
        self.download_file("http://commondatastorage.googleapis.com/git-repo-downloads/repo",
                           file_name="repo", parent_dir=self.bindir)
        repo = os.path.join(self.bindir, "repo")
        self.chmod(repo, 0755)

        self.mkdir_p(self.aospdir)
        self.run_command([repo, "init", "-u", self.config['android_url'],
                          "-b", self.tag],
                         cwd=self.aospdir,
                         halt_on_failure=True)
        self.run_command([repo, "sync", "-j", str(self.ncores)],
                         cwd=self.aospdir,
                         halt_on_failure=True)

        # Note: AOSP upstream tagged a version of the emulator with 2.3.x that will not
        # actually boot gingerbread images. Quoting their explanation for posterity here, from
        # http://source.android.com/source/known-issues.html:
        #
        #     Symptom: The emulator built directly from the gingerbread branch doesn't start and
        #     stays stuck on a black screen.
        #
        #     Cause: The gingerbread branch uses version R7 of the emulator, which doesn't have all
        #     the features necessary to run recent versions of gingerbread.
        #
        #     Fix: Use version R12 of the emulator, and a newer kernel that matches those tools. No
        #     need to do a clean build.
        #
        if self.tag.startswith("android-2.3"):
            self.info("updating QEMU sub-repository to R12")
            self.info("to compensate for known 2.3.x-incompatible R7 in-tree")
            self.run_command([repo, "forall", "platform/external/qemu",
                              "-c", "git", "checkout", "aosp/tools_r12"],
                             cwd=self.aospdir,
                             halt_on_failure=True)

        # For 2.3.x you _probably_ want to use the symbolic tag 'gingerbread' because it is a
        # post-release development branch on which they have backported the GL emulation that
        # fennec relies on. In order to support that, we need to update qemu to R17.
        if self.tag == 'gingerbread':
            self.info("updating QEMU sub-repository to R17")
            self.info("to acquire machinery needed for GL emulation backport")
            self.run_command([repo, "forall", "platform/external/qemu",
                              "-c", "git", "checkout", "aosp/tools_r17"],
                             cwd=self.aospdir,
                             halt_on_failure=True)

    def download_kernel(self):
        self.mkdir_p(self.workdir)
        self.run_command(["git", "clone",
                          "https://android.googlesource.com/kernel/goldfish.git"],
                         cwd=self.workdir,
                         halt_on_failure=True)
        self.run_command(["git", "checkout",
                          "origin/android-goldfish-2.6.29"],
                         cwd=self.goldfishdir,
                         halt_on_failure=True)


    def download_ndk(self):
        ndk = "android-ndk-%s-linux-%s.tar.bz2" % (self.config['ndk_version'], self.config['host_arch'])
        self.download_file("http://dl.google.com/android/ndk/" + ndk,
                           file_name=ndk, parent_dir=self.workdir)
        self.run_command(["tar", "-xjf", ndk],
                         cwd=self.workdir, halt_on_failure=True)


    def download_test_binaries(self):
            lines = []
            zipname = None
            host = "ftp.mozilla.org"
            path = None
            if self.is_arm_target():
                path = 'pub/mobile/nightly/latest-mozilla-central-android'
            else:
                path = 'pub/mobile/nightly/latest-mozilla-central-android-x86'
            ftp = FTP(host)
            ftp.login()
            ftp.cwd(path)
            ftp.retrlines('NLST', lambda x: lines.append(x.strip()))
            for line in lines:
                if line.endswith("tests.zip"):
                    zipname = line
                    break
            if zipname == None:
                self.fatal("unable to find *tests.zip at ftp://%s/%s" % (host,path))
            url = "ftp://%s/%s/%s" % (host,path,zipname)
            self.download_file(url, file_name=zipname, parent_dir=self.workdir)
            self.run_command(["unzip", zipname,
                              "bin/sutAgentAndroid.apk",
                              "bin/Watcher.apk"],
                             cwd=self.workdir,
                             halt_on_failure=True)


    def checkout_orangutan(self):
        self.mkdir_p(self.workdir)
        self.run_command(["git", "clone",
                          "https://github.com/wlach/orangutan.git"],
                         cwd=self.workdir,
                         halt_on_failure=True)


    def patch_aosp(self):
        if self.patches != None:
            for patch in self.patches:
                projectdir = patch[0]
                url = patch[1]
                patchdir = os.path.join(self.aospdir, projectdir)
                self.info("downloading and applying AOSP patch %s to %s" % (url, patchdir))
                self.download_file(url,
                                   file_name='aosp.patch',
                                   parent_dir=self.workdir)
                self.run_command(['patch', '-p1',
                                  '-i', os.path.join(self.workdir, 'aosp.patch')],
                                 cwd=patchdir,
                                 halt_on_failure=True)

    def build_aosp(self):

        arch = None
        variant = None
        abi = None
        abi2 = ""
        if self.is_arm_target():
            arch = "arm"
            if self.is_armv7_target():
                variant = "armv7-a"
                abi = "armeabi"
                abi2 = "armeabi-v7a"
            else:
                variant = "armv5te"
                abi = "armeabi"
        else:
            arch = "x86"
            variant = "x86"
            abi = "x86"

        if abi2 != "":
            abi2 = " TARGET_CPU_ABI2=" + abi2

        env = { "BUILD_EMULATOR_OPENGL": "true",
                "BUILD_EMULATOR_OPENGL_DRIVER": "true" }

        self.run_command(["/bin/bash", "-c",
                          ". build/envsetup.sh "
                          "&& lunch sdk-eng "
                          "&& make -j " + str(self.ncores) +
                          " sdk" +
                          " TARGET_ARCH=" + arch +
                          " TARGET_ARCH_VARIANT=" + variant +
                          " TARGET_CPU_ABI=" + abi +
                          abi2 +
                          " CC=gcc-4.4 CXX=g++-4.4" +
                          " && make out/host/linux-x86/bin/mksdcard"],
                         cwd=self.aospdir,
                         halt_on_failure=True,
                         partial_env=env)

    def build_kernel(self):
        env = {}
        targ = 'goldfish_defconfig'
        if self.is_arm_target():
            env['ARCH']='arm'
            env['PATH']=(self.ndk_bin_dir() +
                         ':/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin')
            env['CROSS_COMPILE']='arm-linux-androideabi-'
            if self.is_armv7_target():
                env['SUBARCH']='armv7'
                targ='goldfish_armv7_defconfig'
            else:
                env['SUBARCH']='armv5'
        else:
            env['ARCH']='x86'
        self.run_command(["make", targ], cwd=self.goldfishdir,
                         halt_on_failure=True, partial_env=env)
        self.run_command(["make", "-j", str(self.ncores)], cwd=self.goldfishdir,
                         halt_on_failure=True, partial_env=env)


    def build_orangutan_su(self):
        self.run_command([self.ndk_bin("gcc"),
                          "--sysroot", self.ndk_sysroot(),
                          "-fPIC", "-mandroid", "-o", "su", "su.c"],
                         cwd=os.path.join(self.workdir, "orangutan"),
                         halt_on_failure=True)


    def write_registry_file(self, avddir, avdname):

        # Write the (mostly redundant) file that registers the AVD
        self.info("writing %s.ini AVD-registry file pointing to %s/avd/%s.avd"
                  % (avdname, avddir, avdname))
        reg = "avd.ini.encoding=ISO-8859-1\n"
        reg += "target=android-%s\n" % self.apilevel
        reg += "path=%s\n" % os.path.abspath(avddir)
        reg += "path.rel=avd/%s.avd\n" % avdname
        self.write_to_file(os.path.join(self.androiddir, "avd", avdname + ".ini"),
                           reg, create_parent_dir=True)

    def make_one_avd(self, avdname):

        named_avddir = os.path.join(self.avddir, avdname + ".avd")
        self.write_registry_file(named_avddir, avdname)
        self.mkdir_p(named_avddir)

        # Write the foo.avd/config.ini file that defines the AVD
        self.info("writing %s.avd/config.ini that defines the AVD" % avdname)
        ini = CONFIG_INI.copy()
        ini["image.sysdir.1"] = "platforms/android-" + self.apilevel + "/images/"
        if self.is_arm_target():
            ini["hw.cpu.arch"] = "arm"
            if self.is_armv7_target():
                ini["abi.type"] = "armeabi-v7a"
            else:
                ini["abi.type"] = "armeabi"
        else:
            ini["hw.cpu.arch"] = "x86"
            ini["abi.type"] = "x86"
            for i in ["bios.bin", "vgabios-cirrus.bin"]:
                self.copyfile(os.path.join(self.aospdir, "prebuilts/qemu-kernel/x86/pc-bios", i),
                              os.path.join(named_avddir, i))

        ini_file = open(os.path.join(named_avddir, "config.ini"), "w")
        for (k,v) in ini.items():
            ini_file.write("%s=%s\n" % (k,v))
        ini_file.close()

        # Copy the per-AVD filesystem images into place
        self.info("copying userdata images from AOSP build to AVD")
        for i in ["system.img", "ramdisk.img", "userdata.img", "userdata-qemu.img"]:
            self.copyfile(os.path.join(self.aospprodoutdir, i),
                          os.path.join(named_avddir, i))

        self.info("making 500M %s/sdcard.img" % named_avddir)
        self.run_command([os.path.join(self.aosphostbindir, "mksdcard"),
                          "500M",
                          os.path.join(named_avddir, "sdcard.img")],
                         halt_on_failure=True)

        # Copy the kernel into place
        self.info("copying kernel from goldfish build dir into %s" % self.avddir)
        kern = None
        if self.is_arm_target():
            kpath = "arch/arm/boot"
        else:
            kpath = "arch/x86/boot"
        kpath = os.path.join(self.goldfishdir, kpath)
        for i in ["zImage", "bzImage"]:
            if os.path.exists(os.path.join(kpath,i)):
                kern = os.path.join(kpath,i)
                break
        self.copyfile(os.path.join(self.goldfishdir, kern),
                      os.path.join(named_avddir, "kernel-qemu"))


    def make_base_avd(self):

        # Write the devices.xml file that defines 'mozilla-device'
        self.info("writing devices.xml that contains mozilla-device")
        self.write_to_file(os.path.join(self.androiddir, "devices.xml"),
                           MOZILLA_DEVICE_DEFINITION, create_parent_dir=True)

        self.make_one_avd("test-1")

    def emu_env(self):
        partial_env = { "ANDROID_SDK_HOME": self.workdir,
                        "ANDROID_PRODUCT_OUT": self.aospprodoutdir }
        return self.query_env(partial_env=partial_env)

    def cpu_specific_args(self, avddir):
        args = []
        if self.is_armv7_target():
            args = ["-qemu", "-cpu", "cortex-a8"]
        else:
            # point to avddir to pick up x86 BIOS
            args = ["-qemu", "-L", avddir]
        return args


    def customize_avd(self):

        self.info("starting emulator for customization run")
        args = [self.emu, "-avd", "test-1", "-no-window", "-gpu", "off", "-partition-size", "1024"]

        # unknown reason, on AWS the 64bit emulators don't seem to enjoy working;
        # for now we punt to 32bit any time we might hit a 64bit one by accident.
        if (os.path.exists(os.path.join(self.aosphostbindir, "emulator64-arm")) or
            os.path.exists(os.path.join(self.aosphostbindir, "emulator64-x86"))):
            args += ["-force-32bit"]

        # CPU-specific flags fall through to qemu, so come last
        args += self.cpu_specific_args(os.path.join(self.avddir, "test-1.avd"))

        self.info("starting emulator: " + subprocess.list2cmdline(args))
        self.info("in dir: %s" % self.aospdir)
        self.info("in env: %s" % pprint.pformat(self.emu_env()))

        p = subprocess.Popen(args,
                             cwd=self.aospdir,
                             env=self.emu_env())

        self.info("waiting for device to start")
        self.adb_e(["wait-for-device"])

        # Wait until the package manager is online as well.
        self.info("waiting for package manager to respond")
        while True:
            time.sleep(10)
            f = self.get_output_from_command([self.adb, "-e", "shell",
                                              "pm", "path", "android"],
                                             halt_on_failure=False)
            if f.startswith("package:"):
                time.sleep(10)
                break

        self.info("modifying 'su' on emulator")
        self.adb_e(["shell", "mount", "-o", "remount,rw", "/dev/block/mtdblock0", "/system"])
        self.adb_e(["push", os.path.join(self.workdir, "orangutan/su"), "/system/xbin"])
        self.adb_e(["shell", "chmod", "6755", "/system/xbin/su"])
        self.adb_e(["shell", "mount", "-o", "remount,ro", "/dev/block/mtdblock0", "/system"])

        self.info("installing packages into emulator")
        self.adb_e(["install", os.path.join(self.bindir, "Watcher.apk")])
        self.adb_e(["install", os.path.join(self.bindir, "sutAgentAndroid.apk")])

        self.info("starting watcher and SUTagent")
        self.adb_e(["shell", "am", "start", "-W", "com.mozilla.watcher/.WatcherMain"])
        self.adb_e(["shell", "am", "start", "-W", "com.mozilla.SUTAgentAndroid/.SUTAgentAndroid"])

        self.info("copying customized emulator system image from /tmp/")

        sys_image = None
        d = "/proc/%d/fd" % p.pid
        for i in os.listdir(d):
            path = os.readlink(os.path.join(d, i))
            if re.match("/tmp/android-.*/emulator-.*", path):
                if sys_image != None:
                    self.fatal("multiple plausible emulator system images, "
                               "kill emulators and try again")
                sys_image = path

        if sys_image == None:
            self.fatal("unable to find running emulator's system image")

        tmp = os.path.join(self.workdir, "system.img")
        self.copyfile(sys_image, tmp)
        self.info("terminating emulator")
        p.terminate()
        p.poll()
        self.info("moving customized system.img into %s/test-1.avd" % self.avddir)
        self.move(tmp, os.path.join(self.avddir, "test-1.avd", "system.img"))


    def adb_e(self, commands):
        self.run_command([self.adb, "-e"] + commands,
                         cwd=self.workdir,
                         halt_on_failure=True)


    def clone_customized_avd(self):
        if self.navds < 2:
            self.info("less than 2 AVDs requested, not cloning")
            return

        srcdir = os.path.join(self.avddir, "test-1.avd")
        for i in ["test-%d" % n for n in range(2,self.navds+1)]:
            self.info("making basic AVD " + i)
            self.make_one_avd(i)
            self.info("overwriting %s.avd images with customized images from test-1.avd" % i)
            dstdir = os.path.join(self.avddir, i + ".avd")
            for f in ["system.img", "userdata.img", "userdata-qemu.img"]:
                self.copyfile(os.path.join(srcdir, f),
                              os.path.join(dstdir, f))


    def bundle_avds(self):

        # clear out the cached / auto-generated .ini files in test-1.avd
        for i in ["emulator-user.ini", "hardware-qemu.ini"]:
            p = os.path.join(self.avddir, "test-1.avd", i)
            if os.path.exists(p):
                os.remove(p)

        self.info("rewriting AVD registry files to point to install target dir %s" %
                  self.config['install_android_dir'])
        for i in ["test-%d" % n for n in range(1,self.navds+1)]:
            avddir = os.path.join(self.config['install_android_dir'], "avd", i + ".avd")
            self.write_registry_file(avddir, i)

        filename = ("AVDs-%s-%s-build-%s-%s.tar.gz"
                    % (self.config['target_arch'],
                       self.tag,
                       datetime.date.today().isoformat(),
                       getpass.getuser()))
        self.run_command(["tar", "-czf", filename, "-C", self.androiddir,
                          "devices.xml", "avd"],
                         cwd=self.workdir,
                         halt_on_failure=True)


    def bundle_emulators(self):
        filename = ("emulators-%s-%s-build-%s-%s.tar.gz"
                    % (self.config['target_arch'],
                       self.tag,
                       datetime.date.today().isoformat(),
                       getpass.getuser()))
        emuarch = "x86"
        if self.is_arm_target():
            emuarch = "arm"
        self.run_command(["tar", "-czf", filename, "-C", self.aosphostdir,
                          "bin/emulator", "bin/emulator-" + emuarch, "bin/adb"],
                         cwd=self.workdir,
                         halt_on_failure=True)


    def apt_update(self):
        self.run_command(['sudo', 'apt-get', 'update'],
                         halt_on_failure=True)


    def apt_add_repo(self, repo):
        self.run_command(['sudo', 'apt-add-repository', '-y', repo],
                         halt_on_failure=True)


    def apt_get(self, pkgs):
        self.run_command(['sudo', 'apt-get', 'install', '-y'] + pkgs,
                         halt_on_failure=True)


    def is_arm_target(self):
        return self.config['target_arch'].startswith('arm')


    def is_armv7_target(self):
        return self.config['target_arch'].startswith('armv7')


    def ndk_bin_dir(self):
        if self.is_arm_target():
            return os.path.join(self.ndkdir, "toolchains/arm-linux-androideabi-4.6/prebuilt/linux-%s/bin" % self.config['host_arch'])
        else:
            return os.path.join(self.ndkdir, "toolchains/x86-4.6/prebuilt/linux-%s/bin" % self.config['host_arch'])


    def ndk_sysroot(self):
        arch = "x86"
        if self.is_arm_target():
            arch = "arm"
        apilevel = int(self.apilevel)
        while apilevel > 0:
            p = os.path.join(self.ndkdir, "platforms", "android-" + str(apilevel), "arch-" + arch)
            if os.path.exists(p):
                return p
            apilevel = int(apilevel) - 1
        self.fatal("no NDK sysroot for API level %s or less" % self.apilevel)


    def ndk_cross_prefix(self):
        if self.is_arm_target():
            return "arm-linux-androideabi-"
        else:
            return "i686-linux-android-"


    def ndk_bin(self, b):
        return os.path.join(self.ndk_bin_dir(), self.ndk_cross_prefix() + b)


    def android_apilevel(self, tag):
        pairs = [("1.6", "4"),
                 ("2.0", "5"),
                 ("2.0.1", "6"),
                 ("2.1", "7"),
                 ("2.2", "8"),
                 ("2.3", "9"),
                 ("2.3.3", "10"),
                 ("gingerbread", "10"),
                 ("3.0", "11"),
                 ("3.1", "12"),
                 ("3.2", "13"),
                 ("4.0", "14"),
                 ("4.0.3", "15"),
                 ("4.1", "16"),
                 ("4.2", "17"),
                 ("4.3", "18")]
        for (vers, api) in pairs:
            if tag == vers or tag.startswith("android-" + vers):
                self.info("android tag '%s', vers %s, uses API level %s" % (tag, vers, api))
                return api
        self.fatal("android tag '%s' doesn't map to a known API level" % tag)

    def select_android_tag(self, vers):
        tags = [
            "android-1.6_r1.1",
            "android-1.6_r1.2",
            "android-1.6_r1.3",
            "android-1.6_r1.4",
            "android-1.6_r1.5",
            "android-2.0_r1",
            "android-2.0.1_r1",
            "android-2.1_r1",
            "android-2.1_r2",
            "android-2.1_r2.1p",
            "android-2.1_r2.1s",
            "android-2.1_r2.1p2",
            "android-2.2_r1",
            "android-2.2_r1.1",
            "android-2.2_r1.2",
            "android-2.2_r1.3",
            "android-2.2.1_r1",
            "android-2.2.1_r2",
            "android-2.2.2_r1",
            "android-2.2.3_r1",
            "android-2.2.3_r2",
            "android-2.3_r1",
            "android-2.3.1_r1",
            "android-2.3.2_r1",
            "android-2.3.3_r1",
            "android-2.3.3_r1.1",
            "android-2.3.4_r0.9",
            "android-2.3.4_r1",
            "android-2.3.5_r1",
            "android-2.3.6_r0.9",
            "android-2.3.6_r1",
            "android-2.3.7_r1",
            "android-4.0.1_r1",
            "android-4.0.1_r1.1",
            "android-4.0.1_r1.2",
            "android-4.0.2_r1",
            "android-4.0.3_r1",
            "android-4.0.3_r1.1",
            "android-4.0.4_r1",
            "android-4.0.4_r1.1",
            "android-4.0.4_r1.2",
            "android-4.0.4_r2",
            "android-4.0.4_r2.1",
            "android-4.1.1_r1",
            "android-4.1.1_r1.1",
            "android-4.1.1_r2",
            "android-4.1.1_r3",
            "android-4.1.1_r4",
            "android-4.1.1_r5",
            "android-4.1.1_r6",
            "android-4.1.1_r6.1",
            "android-4.1.2_r1",
            "android-4.1.2_r2",
            "android-4.1.2_r2.1",
            "android-4.2_r1",
            "android-4.2.1_r1",
            "android-4.2.1_r1.1",
            "android-4.2.1_r1.2",
            "android-4.2.2_r1",
            "android-4.2.2_r1.1",
            "android-4.2.2_r1.1",
            "android-4.3_r0.9",
            "android-4.3_r0.9.1",
            "android-4.3_r1",
            "android-4.3_r1.1",
            "android-4.3_r2",
            "android-4.3_r2.1",
            "android-4.3_r2.2",
            "android-4.3_r2.3",
            "android-4.3_r3",
            "android-4.3.1_r1",
        ]
        codenames = {
            "jb": "4.3",
            "jellybean": "4.3",
            "ics": "4.0",
            "gb": "2.3",
            "gingerbread": "2.3",
            "froyo": "2.2",
        }

        # NB: Gingerbread is special: we don't infer to the last release branch, we infer
        # to the _development_ branch 'gingerbread' because it's where the GL emulator
        # was backported to.
        if vers == 'gingerbread':
            self.info("selecting development-branch 'gingerbread' for version '2.3'")
            return 'gingerbread'

        if vers.startswith('2.3'):
            self.info("android version 2.3.x specified, when dev branch 'gingerbread' is most likely")
            self.info("required to get a working fennec build; pass --android-tag if you are certain")
            self.info("you want a non-'gingerbread' 2.3.x version")
            self.fatal("passed 2.3.x for --android-version, failing conservatively")

        if vers in codenames:
            self.info("using android version '%s' for requested codename '%s'" % (codenames[vers], vers))
            vers = codenames[vers]

        for tag in reversed(tags):
            if tag.startswith(vers) or tag.startswith("android-" + vers):
                self.info("selecting tag '%s' for version '%s'" % (tag, vers))
                return tag
        self.fatal("requested android version '%s' doesn't match any tag" % vers)

    def select_patches(self, tag):
        if tag == 'gingerbread':
            # FIXME: perhaps put this patch someplace more stable than bugzilla?
            return [('development',
                     'https://bug910092.bugzilla.mozilla.org/attachment.cgi?id=8361456'),
                    ('external/sqlite',
                     'https://bug910092.bugzilla.mozilla.org/attachment.cgi?id=8364687')]
        return None

    def _post_fatal(self, message=None, exit_code=None):
        for i in ['emulator', 'emulator-arm', 'emulator-x86',
                  'emulator64', 'emulator64-arm', 'emulator64-x86']:
            self._kill_processes(i)


# main
if __name__ == '__main__':
    myScript = EmulatorBuild()
    myScript.run_and_exit()
