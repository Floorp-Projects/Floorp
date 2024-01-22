#!/usr/bin/env python3
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Builds the Bergamot translations engine for integration with Firefox.

If you wish to test the Bergamot engine locally, then uncomment the .wasm line in
the toolkit/components/translations/jar.mn after building the file. Just make sure
not to check the code change in.
"""

import argparse
import multiprocessing
import os
import shutil
import subprocess
from collections import namedtuple

import yaml

DIR_PATH = os.path.realpath(os.path.dirname(__file__))
THIRD_PARTY_PATH = os.path.join(DIR_PATH, "thirdparty")
MOZ_YAML_PATH = os.path.join(DIR_PATH, "moz.yaml")
PATCHES_PATH = os.path.join(DIR_PATH, "patches")
BERGAMOT_PATH = os.path.join(THIRD_PARTY_PATH, "bergamot-translator")
MARIAN_PATH = os.path.join(BERGAMOT_PATH, "3rd_party/marian-dev")
GEMM_SCRIPT = os.path.join(BERGAMOT_PATH, "wasm/patch-artifacts-import-gemm-module.sh")
BUILD_PATH = os.path.join(THIRD_PARTY_PATH, "build-wasm")
EMSDK_PATH = os.path.join(THIRD_PARTY_PATH, "emsdk")
EMSDK_ENV_PATH = os.path.join(EMSDK_PATH, "emsdk_env.sh")
WASM_PATH = os.path.join(BUILD_PATH, "bergamot-translator-worker.wasm")
JS_PATH = os.path.join(BUILD_PATH, "bergamot-translator-worker.js")
FINAL_JS_PATH = os.path.join(DIR_PATH, "bergamot-translator.js")
ROOT_PATH = os.path.join(DIR_PATH, "../../../..")

# 3.1.47 had an error compiling sentencepiece.
EMSDK_VERSION = "3.1.8"
EMSDK_REVISION = "2346baa7bb44a4a0571cc75f1986ab9aaa35aa03"

patches = [
    (BERGAMOT_PATH, os.path.join(PATCHES_PATH, "allocation-bergamot.patch")),
    (MARIAN_PATH, os.path.join(PATCHES_PATH, "allocation-marian.patch")),
]

parser = argparse.ArgumentParser(
    description=__doc__,
    # Preserves whitespace in the help text.
    formatter_class=argparse.RawTextHelpFormatter,
)
parser.add_argument(
    "--clobber", action="store_true", help="Clobber the build artifacts"
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="Build with debug symbols, useful for profiling",
)

ArgNamespace = namedtuple("ArgNamespace", ["clobber", "debug"])


def git_clone_update(name: str, repo_path: str, repo_url: str, revision: str):
    if not os.path.exists(repo_path):
        print(f"\n⬇️ Clone the {name} repo into {repo_path}\n")
        subprocess.check_call(
            ["git", "clone", repo_url],
            cwd=THIRD_PARTY_PATH,
        )

    local_head = subprocess.check_output(
        ["git", "rev-parse", "HEAD"],
        cwd=repo_path,
        text=True,
    ).strip()

    def run(command):
        return subprocess.check_call(command, cwd=repo_path)

    if local_head != revision:
        print(f"The head ({local_head}) and revision ({revision}) don't match.")
        print(f"\n🔎 Fetching the latest from {name}.\n")
        run(["git", "fetch", "--recurse-submodules"])

        print(f"🛒 Checking out the revision {revision}")
        run(["git", "checkout", revision])
        run(["git", "submodule", "update", "--init", "--recursive"])


def install_and_activate_emscripten(args: ArgNamespace):
    git_clone_update(
        name="emsdk",
        repo_path=EMSDK_PATH,
        repo_url="https://github.com/emscripten-core/emsdk.git",
        revision=EMSDK_REVISION,
    )

    # Run these commands in the shell so that the configuration is saved.
    def run_shell(command):
        return subprocess.run(command, cwd=EMSDK_PATH, shell=True, check=True)

    print(f"\n🛠️ Installing EMSDK version {EMSDK_VERSION}\n")
    run_shell("./emsdk install " + EMSDK_VERSION)

    print("\n🛠️ Activating emsdk\n")
    run_shell("./emsdk activate " + EMSDK_VERSION)


def install_bergamot():
    with open(MOZ_YAML_PATH, "r", encoding="utf8") as file:
        text = file.read()

    moz_yaml = yaml.safe_load(text)

    git_clone_update(
        name="bergamot",
        repo_path=BERGAMOT_PATH,
        repo_url=moz_yaml["origin"]["url"],
        revision=moz_yaml["origin"]["revision"],
    )


def to_human_readable(size):
    """Convert sizes to human-readable format"""
    size_in_mb = size / 1048576
    return f"{size_in_mb:.2f}M ({size} bytes)"


def apply_git_patch(repo_path, patch_path):
    print(f"Applying patch {patch_path} to {os.path.basename(repo_path)}")
    subprocess.check_call(["git", "apply", "--reject", patch_path], cwd=repo_path)


def revert_git_patch(repo_path, patch_path):
    print(f"Reverting patch {patch_path} from {os.path.basename(repo_path)}")
    subprocess.check_call(["git", "apply", "-R", "--reject", patch_path], cwd=repo_path)


def build_bergamot(args: ArgNamespace):
    if args.clobber and os.path.exists(BUILD_PATH):
        shutil.rmtree(BUILD_PATH)

    if not os.path.exists(BUILD_PATH):
        os.mkdir(BUILD_PATH)

    print("\n 🖌️ Applying source code patches\n")
    for repo_path, patch_path in patches:
        apply_git_patch(repo_path, patch_path)

    # These commands require the emsdk environment variables to be set up.
    def run_shell(command):
        if '"' in command or "'" in command:
            raise Exception("This run_shell utility does not support quotes.")

        return subprocess.run(
            # "source" is not available in all shells so explicitly
            f"bash -c 'source {EMSDK_ENV_PATH} && {command}'",
            cwd=BUILD_PATH,
            shell=True,
            check=True,
        )

    try:
        flags = ""
        if args.debug:
            flags = "-DCMAKE_BUILD_TYPE=RelWithDebInfo"

        print("\n 🏃 Running CMake for Bergamot\n")
        run_shell(
            f"emcmake cmake -DCOMPILE_WASM=on -DWORMHOLE=off {flags} {BERGAMOT_PATH}"
        )

        print("\n 🏃 Building Bergamot with emmake\n")
        run_shell(f"emmake make -j {multiprocessing.cpu_count()}")

        print("\n 🪚 Patching Bergamot for gemm support\n")
        subprocess.check_call(["bash", GEMM_SCRIPT, BUILD_PATH])

        print("\n✅ Build complete\n")
        print("  " + JS_PATH)
        print("  " + WASM_PATH)

        # Get the sizes of the build artifacts.
        wasm_size = os.path.getsize(WASM_PATH)
        gzip_size = int(
            subprocess.run(
                f"gzip -c {WASM_PATH} | wc -c",
                check=True,
                shell=True,
                capture_output=True,
            ).stdout.strip()
        )
        print(f"  Uncompressed wasm size: {to_human_readable(wasm_size)}")
        print(f"  Compressed wasm size: {to_human_readable(gzip_size)}")
    finally:
        print("\n🖌️ Reverting the source code patches\n")
        for repo_path, patch_path in patches[::-1]:
            revert_git_patch(repo_path, patch_path)


def write_final_bergamot_js_file():
    """
    The generated JS file requires some light patching for integration.
    """

    source = "\n".join(
        [
            "/* This Source Code Form is subject to the terms of the Mozilla Public",
            " * License, v. 2.0. If a copy of the MPL was not distributed with this",
            " * file, You can obtain one at http://mozilla.org/MPL/2.0/. */",
            "",
            "function loadBergamot(Module) {",
            "",
        ]
    )

    with open(JS_PATH, "r", encoding="utf8") as file:
        for line in file.readlines():
            source += "  " + line

    source += "  return Module;\n}"

    # Use the Module's printing.
    source = source.replace("console.log(", "Module.print(")

    # Add some instrumentation to the module's memory size.
    source = source.replace(
        "function updateGlobalBufferAndViews(buf) {",
        """
        function updateGlobalBufferAndViews(buf) {
          const mb = (buf.byteLength / 1_000_000).toFixed();
          Module.print(
            `Growing wasm buffer to ${mb}MB (${buf.byteLength} bytes).`
          );
    """,
    )

    print("\n Formatting the final bergamot file")
    # Create the file outside of this directory so it's not ignored by eslint.
    temp_path = os.path.join(DIR_PATH, "../temp-bergamot.js")
    with open(temp_path, "w", encoding="utf8") as file:
        file.write(source)

    subprocess.run(
        f"./mach eslint --fix {temp_path}",
        cwd=ROOT_PATH,
        check=True,
        shell=True,
        capture_output=True,
    )

    print(f"\n Writing out final bergamot file: {FINAL_JS_PATH}")
    shutil.move(temp_path, FINAL_JS_PATH)


def main():
    args: ArgNamespace = parser.parse_args()

    if not os.path.exists(THIRD_PARTY_PATH):
        os.mkdir(THIRD_PARTY_PATH)

    install_and_activate_emscripten(args)
    install_bergamot()
    build_bergamot(args)
    write_final_bergamot_js_file()


if __name__ == "__main__":
    main()
