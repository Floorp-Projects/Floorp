# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import platform
import subprocess
from subprocess import CalledProcessError

from mach.site import PythonVirtualenv
from mach.util import get_state_dir
from mozfile import which


MINIMUM_RUST_VERSION = "1.53.0"


def get_tools_dir(srcdir=False):
    if os.environ.get("MOZ_AUTOMATION") and "MOZ_FETCHES_DIR" in os.environ:
        return os.environ["MOZ_FETCHES_DIR"]
    return get_state_dir(srcdir)


def get_mach_virtualenv_root():
    return os.path.join(
        get_state_dir(specific_to_topsrcdir=True), "_virtualenvs", "mach"
    )


def get_mach_virtualenv_binary():
    root = get_mach_virtualenv_root()
    return PythonVirtualenv(root).python_path


class JavaLocationFailedException(Exception):
    pass


def locate_java_bin_path():
    """Locate an expected version of Java.

    We require an installation of Java that is:
    * a JDK (not just a JRE) because we compile Java.
    * version 1.8 because the Android `sdkmanager` tool still needs it.
    """

    if "JAVA_HOME" in os.environ:
        java_home = os.environ["JAVA_HOME"]
        bin_path = os.path.join(java_home, "bin")
        java_path = which("java", path=bin_path)
        javac_path = which("javac", path=bin_path)

        if not java_path:
            raise JavaLocationFailedException(
                'The $JAVA_HOME environment variable ("{}") is not '
                "pointing to a valid Java installation. Please "
                "change $JAVA_HOME.".format(java_home)
            )

        version = _resolve_java_version(java_path)

        if not _is_java_version_correct(version):
            raise JavaLocationFailedException(
                'The $JAVA_HOME environment variable ("{}") is '
                'pointing to a Java installation with version "{}". '
                "Howevever, Firefox depends on version 1.8. Please "
                "change $JAVA_HOME.".format(java_home, version)
            )

        if not javac_path:
            raise JavaLocationFailedException(
                'The $JAVA_HOME environment variable ("{}") is '
                'pointing to a "JRE". Since Firefox depends on Java '
                'tools that are bundled in a "JDK", you will need '
                "to update $JAVA_HOME to point to a Java 1.8 JDK "
                "instead (You may need to install a JDK first).".format(java_home)
            )

        return bin_path

    system = platform.system()
    if system == "Windows":
        jdk_bin_path = (
            _windows_registry_get_adopt_open_jdk_8_path()
            or _windows_registry_get_oracle_jdk_8_path()
        )

        if not jdk_bin_path:
            raise JavaLocationFailedException(
                "Could not find the Java 1.8 JDK on your machine. "
                "Please install it: "
                "https://adoptopenjdk.net/?variant=openjdk8"
            )

        # No need for version/type validation: the registry keys we're looking up correspond to
        # the specific Java installations we want.
        return jdk_bin_path
    elif system == "Darwin":
        no_matching_java_version_exception = JavaLocationFailedException(
            'Could not find Java on your machine. Please install it, either via "mach bootstrap" '
            "(if you have brew) or directly from https://adoptopenjdk.net/?variant=openjdk8."
            "If you have already installed a 1.8 JDK, you may need to set $JAVA_HOME"
        )
        # In some cases, such as reported in bug 1670264, the
        # "java_home" binary might return a JRE instead of a JDK,
        # when the JRE has a higher priority (e.g. after a system
        # upgrade that re-installs the JRE). To make sure that the
        # expected JDK is found if it exists, we read the verbose
        # output of java_home in XML format and extract a fitting
        # path.
        try:
            java_home_verbose_xml_output = subprocess.check_output(
                ["/usr/libexec/java_home", "-VX", "-v", "1.8"], stderr=subprocess.PIPE
            )
            import plistlib

            list_of_java_homes = plistlib.loads(java_home_verbose_xml_output)
        except CalledProcessError:
            raise no_matching_java_version_exception

        # If adoptopenjdk8 is available, use it.
        for java_home_item in list_of_java_homes:
            if java_home_item["JVMBundleID"] == "net.adoptopenjdk.8.jdk":
                return os.path.join(java_home_item["JVMHomePath"], "bin")

        # Fall back to any other JDK 1.8.
        for java_home_item in list_of_java_homes:
            java_home_path = java_home_item["JVMHomePath"]
            bin_path = os.path.join(java_home_path, "bin")
            javac_path = which("javac", path=bin_path)
            if javac_path:
                return bin_path

        raise no_matching_java_version_exception
    else:  # Handle Linux and other OSes by finding Java from the $PATH
        java_path = which("java")
        javac_path = which("javac")
        if not java_path:
            raise JavaLocationFailedException(
                'Could not find "java" on the $PATH. Please install '
                "the Java 1.8 JDK and/or set $JAVA_HOME."
            )

        java_version = _resolve_java_version(java_path)
        if not _is_java_version_correct(java_version):
            raise JavaLocationFailedException(
                'The "java" located on the $PATH has version "{}", '
                "but Firefox depends on version 1.8. Please install "
                "the Java 1.8 JDK and/or set $JAVA_HOME.".format(java_version)
            )

        if not javac_path:
            raise JavaLocationFailedException(
                'Could not find "javac" on the $PATH, even though '
                '"java" was located. This probably means that you '
                "have a JRE installed, not a JDK. Since Firefox "
                "needs to compile Java, you should install the "
                "Java 1.8 JDK and/or set $JAVA_HOME."
            )

        javac_bin_path = os.path.dirname(os.path.realpath(javac_path))
        javac_version = _resolve_java_version(which("java", path=javac_bin_path))

        # On Ubuntu, there's an "update-alternatives" command that can be used to change the
        # default version of specific binaries. To ensure that "java" and "javac" are
        # pointing at the same "alternative", we compare their versions.
        if java_version != javac_version:
            raise JavaLocationFailedException(
                'The "java" ("{}") and "javac" ("{}") binaries on '
                "the $PATH are currently coming from two different "
                "Java installations with different versions. Please "
                "resolve this, or explicitly set $JAVA_HOME.".format(
                    java_version, javac_version
                )
            )

        # We use the "bin/" detected as a parent of "javac" instead of "java" because the
        # structure of some JDKs places "java" in a different directory:
        #
        # $JDK/
        #    bin/
        #        javac
        #        java -> ../jre/bin/java
        #        ...
        #    jre/
        #        bin/
        #            java
        #            ...
        #    ...
        #
        # Realpath-ing "javac" should consistently gives us a JDK bin dir
        # containing both "java" and JDK tools.
        return javac_bin_path


def _resolve_java_version(java_bin):
    output = subprocess.check_output(
        [java_bin, "-XshowSettings:properties", "-version"],
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    ).rstrip()

    # -version strings are pretty free-form, like: 'java version
    # "1.8.0_192"' or 'openjdk version "11.0.1" 2018-10-16', but the
    # -XshowSettings:properties gives the information (to stderr, sigh)
    # like 'java.specification.version = 8'.  That flag is non-standard
    # but has been around since at least 2011.
    version = [
        line for line in output.splitlines() if "java.specification.version" in line
    ]

    if len(version) != 1:
        return None

    return version[0].split(" = ")[-1]


def _is_java_version_correct(version):
    return version in ["1.8", "8"]


def _windows_registry_get_oracle_jdk_8_path():
    try:
        import _winreg
    except ImportError:
        import winreg as _winreg

    try:
        with _winreg.OpenKeyEx(
            _winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\JavaSoft\Java Development Kit\1.8"
        ) as key:
            path, _ = _winreg.QueryValueEx(key, "JavaHome")
            return os.path.join(path, "bin")
    except FileNotFoundError:
        return None


def _windows_registry_get_adopt_open_jdk_8_path():
    try:
        import _winreg
    except ImportError:
        import winreg as _winreg

    try:
        # The registry key name looks like:
        # HKLM\SOFTWARE\AdoptOpenJDK\JDK\8.0.252.09\hotspot\MSI:Path
        #                                ^^^^^^^^^^
        # Due to the very precise version in the path, we can't just OpenKey("<static path>").
        # Instead, we need to enumerate the list of JDKs, find the one that seems to be what
        # we're looking for (JDK 1.8), then get the path from there.
        with _winreg.OpenKeyEx(
            _winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\AdoptOpenJDK\JDK"
        ) as jdk_key:
            index = 0
            while True:
                version_key_name = _winreg.EnumKey(jdk_key, index)
                if not version_key_name.startswith("8."):
                    index += 1
                    continue

                with _winreg.OpenKeyEx(
                    jdk_key, r"{}\hotspot\MSI".format(version_key_name)
                ) as msi_key:
                    path, _ = _winreg.QueryValueEx(msi_key, "Path")
                    return os.path.join(path, "bin")
    except (FileNotFoundError, OSError):
        return None
