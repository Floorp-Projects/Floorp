# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply different expiration dates to different artifacts based on a manifest file (artifacts.yml)
"""
import logging
import os
import sys

import yaml
from taskgraph.transforms.base import TransformSequence
from yaml import YAMLError

from gecko_taskgraph.transforms.job.common import get_expiration
from gecko_taskgraph.util.workertypes import worker_type_implementation

logger = logging.getLogger(__name__)

transforms = TransformSequence()


def read_artifact_manifest(manifest_path):
    """Read the artifacts.yml manifest file and return it."""
    # logger.info(f"The current directory is {os.getcwd()}")
    try:
        with open(manifest_path, "r") as ymlf:
            yml = yaml.safe_load(ymlf.read())
            return yml
    except YAMLError as ye:
        err = 'Failed to parse manifest "{manifest_path}". Invalid Yaml:'
        err += ye
        raise SystemExit(err)
    except FileNotFoundError:
        err = f'Failed to load manifest "{manifest_path}". File not found'
        raise SystemExit(err)
    except PermissionError:
        err = f'Failed to load manifest "{manifest_path}". Permission Error'
        raise SystemExit(err)


@transforms.add
def set_artifact_expiration(config, jobs):
    """Set the expiration for certain artifacts based on a manifest file."""
    """---
       win:
         - build_resources.json: short

       linux:
         - target.crashreporter-symbols-full.tar.zst: medium
    """
    transform_dir = os.path.dirname(__file__)
    manifest = read_artifact_manifest(os.path.join(transform_dir, "artifacts.yml"))

    for job in jobs:
        try:
            platform = job["attributes"]["build_platform"]
        except KeyError:
            err = "Tried to get build_platfrom for job, but it does not exist. Exiting."
            raise SystemExit(err)
        if "worker" in job:
            if "env" in job["worker"]:
                if isinstance(job["worker"]["env"], dict):
                    job["worker"]["env"]["MOZ_ARTIFACT_PLATFORM"] = platform
                else:
                    raise SystemExit(
                        f"Expected env to be a dict, but it was {type(job['worker']['env'])}"
                    )
            if "artifacts" in job["worker"]:
                plat = platform.lower()
                if "plain" in plat or "ccov" in plat or "rusttest" in plat:
                    art_dict = None
                elif (
                    plat == "toolchain-wasm32-wasi-compiler-rt-trunk"
                    or plat == "toolchain-linux64-x64-compiler-rt-trunk"
                    or plat == "toolchain-linux64-x86-compiler-rt-trunk"
                    or plat == "android-geckoview-docs"
                ):
                    art_dict = None
                elif plat.startswith("win"):
                    art_dict = manifest["win"]
                elif plat.startswith("linux"):
                    art_dict = manifest["linux"]
                elif plat.startswith("mac"):
                    art_dict = manifest["macos"]
                elif plat.startswith("android"):
                    art_dict = manifest["android"]
                else:
                    print(
                        'The platform name "{plat}" didn\'t start with',
                        '"win", "mac", "android", or "linux".',
                        file=sys.stderr,
                    )
                    art_dict = None
                worker_implementation, _ = worker_type_implementation(
                    config.graph_config, config.params, job["worker-type"]
                )
                if worker_implementation == "docker-worker":
                    artifact_dest = "/builds/worker/cidata/{}"
                else:
                    artifact_dest = "cidata/{}"

                if art_dict is not None:
                    for art_name in art_dict.keys():
                        # The 'artifacts' key of a job is a list at this stage.
                        # So, must append a new dict to the list
                        expiry_policy = art_dict[art_name]
                        expires = get_expiration(config, policy=expiry_policy)
                        new_art = {
                            "name": f"public/cidata/{art_name}",
                            "path": artifact_dest.format(art_name),
                            "type": "file",
                            "expires-after": expires,
                        }
                        job["worker"]["artifacts"].append(new_art)
        yield job
