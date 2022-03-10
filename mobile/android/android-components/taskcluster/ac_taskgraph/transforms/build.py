# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import datetime
import re

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

from ..build_config import get_version, get_path, get_extensions


transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for field in (
            'attributes.code-review',
            'expose-artifacts',
            'include-coverage',
            'run-on-tasks-for',
            'run.gradlew',
            'treeherder.symbol',
        ):
            resolve_keyed_by(
                task, field, item_name=task["name"],
                **{
                    'build-type': task["attributes"]["build-type"],
                    'component': task["attributes"]["component"],
                }
            )

        yield task


@transforms.add
def handle_coverage(config, tasks):
    for task in tasks:
        if task.pop('include-coverage', False):
            task['run']['gradlew'].insert(0, '-Pcoverage')
        yield task


@transforms.add
def interpolate_missing_values(config, tasks):
    timestamp = _get_timestamp(config)
    version = get_version()
    nightly_version = get_nightly_version(config, version)

    for task in tasks:
        for field in ('description', 'run.gradlew', 'treeherder.symbol'):
            component = task["attributes"]["component"]
            _deep_format(
                task, field, component=component, timestamp=timestamp, nightlyVersion=nightly_version
            )

        yield task


def _get_timestamp(config):
    push_date_string = config.params["moz_build_date"]
    push_date_time = datetime.datetime.strptime(push_date_string, "%Y%m%d%H%M%S")
    return push_date_time.strftime('%Y%m%d.%H%M%S-1')


def _get_buildid(config):
    return config.params.moz_build_date.strftime('%Y%m%d%H%M%S')


def get_nightly_version(config, version):
    buildid = _get_buildid(config)
    # TODO: to replace here with mozilla-version sanity check and parsing
    pattern = re.compile(r"""
        (?P<major_number>\d+)\.(?P<minor_number>\d+)\.(?P<patch_number>\d+)
        """, re.VERBOSE)
    match = pattern.search(version)
    try:
        _ = match.group()
    except AttributeError:
        raise Exception(f'version {version} does not follow semver')
    version_dict = match.groupdict()
    return '{}.{}.{}'.format(
            version_dict['major_number'],
            version_dict['minor_number'],
            buildid
    )


def craft_path_version(version, build_type, nightly_version):
    """Helper function to craft the correct version to bake in the artifacts full
    path section"""
    path_version = version
    # XXX: for nightly releases we need to s/X.0.0/X.0.<buildid>/g in versions
    if build_type == 'nightly':
        path_version = path_version.replace(version, nightly_version)
    return path_version


def _deep_format(object, field, **format_kwargs):
    keys = field.split('.')
    last_key = keys[-1]
    for key in keys[:-1]:
        object = object[key]

    one_before_last_object = object
    object = object[last_key]

    if isinstance(object, str):
        one_before_last_object[last_key] = object.format(**format_kwargs)
    elif isinstance(object, list):
        one_before_last_object[last_key] = [item.format(**format_kwargs) for item in object]
    else:
        raise ValueError(f'Unsupported type for object: {object}')


@transforms.add
def add_artifacts(config, tasks):
    timestamp = _get_timestamp(config)
    version = get_version()
    nightly_version = get_nightly_version(config, version)

    for task in tasks:
        artifact_template = task.pop("artifact-template", {})
        task["attributes"]["artifacts"] = artifacts = {}

        component = task["attributes"]["component"]
        build_artifact_definitions = task.setdefault("worker", {}).setdefault("artifacts", [])

        for key in ["tests-artifact-template", "lint-artifact-template", "text-artifact-template", "jacoco-coverage-template"]:
            if key in task:
                optional_artifact_template = task.pop(key, {})
                build_artifact_definitions.append({
                    "type": optional_artifact_template["type"],
                    "name": optional_artifact_template["name"],
                    "path": optional_artifact_template["path"].format(
                        component_path=get_path(component)
                    ),
                })

        if task.pop("expose-artifacts", False):
            all_extensions = get_extensions(component)
            artifact_file_names_per_extension = {
                extension: '{component}-{version}{timestamp}{extension}'.format(
                    component=component,
                    version=version,
                    timestamp='',
                    extension=extension,
                )
                for extension in all_extensions
            }
            # XXX: rather than adding more complex logic above, we simply post-adjust the
            # dictionary for `nightly` types of graphs
            if task['attributes']['build-type'] == 'nightly':
                for ext, path in artifact_file_names_per_extension.items():
                    if version in path:
                        artifact_file_names_per_extension[ext] = path.replace(version, nightly_version)

            for extension, artifact_file_name in artifact_file_names_per_extension.items():
                artifact_full_name = artifact_template["name"].format(
                    artifact_file_name=artifact_file_name,
                )
                build_artifact_definitions.append({
                    "type": artifact_template["type"],
                    "name": artifact_full_name,
                    "path": artifact_template["path"].format(
                        component_path=get_path(component),
                        component=component,
                        version=craft_path_version(version,
                                task["attributes"]["build-type"], nightly_version),
                        artifact_file_name=artifact_file_name,
                    ),
                })

                artifacts[extension] = artifact_full_name

        yield task
