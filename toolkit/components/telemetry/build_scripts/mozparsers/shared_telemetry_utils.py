# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file contains utility functions shared by the scalars and the histogram generation
# scripts.

import os
import re
import sys

import yaml

# This is a list of flags that determine which process a measurement is allowed
# to record from.
KNOWN_PROCESS_FLAGS = {
    "all": "All",
    "all_children": "AllChildren",
    "main": "Main",
    "content": "Content",
    "gpu": "Gpu",
    "rdd": "Rdd",
    "socket": "Socket",
    "utility": "Utility",
    # Historical Values
    "all_childs": "AllChildren",  # Supporting files from before bug 1363725
}

GECKOVIEW_STREAMING_PRODUCT = "geckoview_streaming"

SUPPORTED_PRODUCTS = {
    "firefox": "Firefox",
    "fennec": "Fennec",
    GECKOVIEW_STREAMING_PRODUCT: "GeckoviewStreaming",
    "thunderbird": "Thunderbird",
    # Historical, deprecated values:
    # 'geckoview': 'Geckoview',
}

SUPPORTED_OPERATING_SYSTEMS = [
    "mac",
    "linux",
    "windows",
    "android",
    "unix",
    "all",
]

# mozinfo identifies linux, BSD variants, Solaris and SunOS as unix
# Solaris and SunOS are identified as "unix" OS.
UNIX_LIKE_OS = [
    "unix",
    "linux",
    "bsd",
]

CANONICAL_OPERATING_SYSTEMS = {
    "darwin": "mac",
    "linux": "linux",
    "winnt": "windows",
    "android": "android",
    # for simplicity we treat all BSD and Solaris systems as unix
    "gnu/kfreebsd": "unix",
    "sunos": "unix",
    "dragonfly": "unix",
    "freeunix": "unix",
    "netunix": "unix",
    "openunix": "unix",
}

PROCESS_ENUM_PREFIX = "mozilla::Telemetry::Common::RecordedProcessType::"
PRODUCT_ENUM_PREFIX = "mozilla::Telemetry::Common::SupportedProduct::"


class ParserError(Exception):
    """Thrown by different probe parsers. Errors are partitioned into
    'immediately fatal' and 'eventually fatal' so that the parser can print
    multiple error messages at a time. See bug 1401612 ."""

    eventual_errors = []

    def __init__(self, *args):
        Exception.__init__(self, *args)

    def handle_later(self):
        ParserError.eventual_errors.append(self)

    def handle_now(self):
        ParserError.print_eventuals()
        print(str(self), file=sys.stderr)
        sys.stderr.flush()
        os._exit(1)

    @classmethod
    def print_eventuals(cls):
        while cls.eventual_errors:
            print(str(cls.eventual_errors.pop(0)), file=sys.stderr)

    @classmethod
    def exit_func(cls):
        if cls.eventual_errors:
            cls("Some errors occurred").handle_now()


def is_valid_process_name(name):
    return name in KNOWN_PROCESS_FLAGS


def process_name_to_enum(name):
    return PROCESS_ENUM_PREFIX + KNOWN_PROCESS_FLAGS.get(name)


def is_valid_product(name):
    return name in SUPPORTED_PRODUCTS


def is_geckoview_streaming_product(name):
    return name == GECKOVIEW_STREAMING_PRODUCT


def is_valid_os(name):
    return name in SUPPORTED_OPERATING_SYSTEMS


def canonical_os(os):
    """Translate possible OS_TARGET names to their canonical value."""

    return CANONICAL_OPERATING_SYSTEMS.get(os.lower()) or "unknown"


def product_name_to_enum(product):
    if not is_valid_product(product):
        raise ParserError("Invalid product {}".format(product))
    return PRODUCT_ENUM_PREFIX + SUPPORTED_PRODUCTS.get(product)


def static_assert(output, expression, message):
    """Writes a C++ compile-time assertion expression to a file.
    :param output: the output stream.
    :param expression: the expression to check.
    :param message: the string literal that will appear if the expression evaluates to
        false.
    """
    print('static_assert(%s, "%s");' % (expression, message), file=output)


def validate_expiration_version(expiration):
    """Makes sure the expiration version has the expected format.

    Allowed examples: "10", "20", "60", "never"
    Disallowed examples: "Never", "asd", "4000000", "60a1", "30.5a1"

    :param expiration: the expiration version string.
    :return: True if the expiration validates correctly, False otherwise.
    """
    if expiration != "never" and not re.match(r"^\d{1,3}$", expiration):
        return False

    return True


def add_expiration_postfix(expiration):
    """Formats the expiration version and adds a version postfix if needed.

    :param expiration: the expiration version string.
    :return: the modified expiration string.
    """
    if re.match(r"^[1-9][0-9]*$", expiration):
        return expiration + ".0a1"

    if re.match(r"^[1-9][0-9]*\.0$", expiration):
        return expiration + "a1"

    return expiration


def load_yaml_file(filename):
    """Load a YAML file from disk, throw a ParserError on failure."""
    try:
        with open(filename, "r") as f:
            return yaml.safe_load(f)
    except IOError as e:
        raise ParserError("Error opening " + filename + ": " + str(e))
    except ValueError as e:
        raise ParserError("Error parsing processes in {}: {}".format(filename, e))
