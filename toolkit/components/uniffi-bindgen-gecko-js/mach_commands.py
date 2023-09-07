# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess

from mach.decorators import Command, SubCommand

# IMPORTANT: Please Request review from a DOM peer before
# committing to using UniFFI. There are other ways to consume Rust from
# JavaScript that might fit your use case better.
UDL_FILES = [
    "third_party/rust/sync15/src/sync15.udl",
    "third_party/rust/tabs/src/tabs.udl",
    "third_party/rust/suggest/src/suggest.udl",
    "third_party/rust/remote_settings/src/remote_settings.udl",
]

FIXTURE_UDL_FILES = [
    "third_party/rust/uniffi-example-geometry/src/geometry.udl",
    "third_party/rust/uniffi-example-arithmetic/src/arithmetic.udl",
    "third_party/rust/uniffi-example-rondpoint/src/rondpoint.udl",
    "third_party/rust/uniffi-example-sprites/src/sprites.udl",
    "third_party/rust/uniffi-example-todolist/src/todolist.udl",
    "toolkit/components/uniffi-fixture-callbacks/src/callbacks.udl",
    "toolkit/components/uniffi-example-custom-types/src/custom-types.udl",
    "toolkit/components/uniffi-fixture-external-types/src/external-types.udl",
]
CPP_PATH = "toolkit/components/uniffi-js/UniFFIGeneratedScaffolding.cpp"
JS_DIR = "toolkit/components/uniffi-bindgen-gecko-js/components/generated"
FIXTURE_CPP_PATH = "toolkit/components/uniffi-js/UniFFIFixtureScaffolding.cpp"
FIXTURE_JS_DIR = "toolkit/components/uniffi-bindgen-gecko-js/fixtures/generated"


def build_uniffi_bindgen_gecko_js(command_context):
    uniffi_root = crate_root(command_context)
    print("Building uniffi-bindgen-gecko-js")
    cmdline = [
        "cargo",
        "build",
        "--release",
        "--manifest-path",
        os.path.join(command_context.topsrcdir, "Cargo.toml"),
        "--package",
        "uniffi-bindgen-gecko-js",
    ]
    subprocess.check_call(cmdline, cwd=uniffi_root)
    print()
    return os.path.join(
        command_context.topsrcdir, "target", "release", "uniffi-bindgen-gecko-js"
    )


@Command(
    "uniffi",
    category="devenv",
    description="Generate JS bindings using uniffi-bindgen-gecko-js",
)
def uniffi(command_context, *runargs, **lintargs):
    """Run uniffi."""
    command_context._sub_mach(["help", "uniffi"])
    return 1


@SubCommand(
    "uniffi",
    "generate",
    description="Generate/regenerate bindings",
)
def generate_command(command_context):
    binary_path = build_uniffi_bindgen_gecko_js(command_context)
    cmdline = [
        binary_path,
        "--js-dir",
        JS_DIR,
        "--fixture-js-dir",
        FIXTURE_JS_DIR,
        "--cpp-path",
        CPP_PATH,
        "--fixture-cpp-path",
        FIXTURE_CPP_PATH,
    ]
    if UDL_FILES:
        cmdline += ["--udl-files"] + UDL_FILES
    if FIXTURE_UDL_FILES:
        cmdline += ["--fixture-udl-files"] + FIXTURE_UDL_FILES
    subprocess.check_call(cmdline, cwd=command_context.topsrcdir)
    return 0


def crate_root(command_context):
    return os.path.join(
        command_context.topsrcdir, "toolkit", "components", "uniffi-bindgen-gecko-js"
    )
