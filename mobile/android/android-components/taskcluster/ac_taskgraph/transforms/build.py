# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import datetime

from six import text_type

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
            task['run']['post-gradlew'] = [['automation/taskcluster/action/upload_coverage_report.sh']]
            task['run']['secrets'] = [{
                'name': 'project/mobile/android-components/public-tokens',
                'key': 'codecov',
                'path': '.cc_token',
            }]

        yield task


@transforms.add
def format_component_name_and_timestamp(config, tasks):
    timestamp = _get_timestamp(config)

    for task in tasks:
        for field in ('description', 'run.gradlew', 'treeherder.symbol'):
            component = task["attributes"]["component"]
            _deep_format(
                task, field, component=component, timestamp=timestamp
            )

        yield task


def _get_timestamp(config):
    push_date_string = config.params["moz_build_date"]
    push_date_time = datetime.datetime.strptime(push_date_string, "%Y%m%d%H%M%S")
    return push_date_time.strftime('%Y%m%d.%H%M%S-1')


def _deep_format(object, field, **format_kwargs):
    keys = field.split('.')
    last_key = keys[-1]
    for key in keys[:-1]:
        object = object[key]

    one_before_last_object = object
    object = object[last_key]

    if isinstance(object, text_type):
        one_before_last_object[last_key] = object.format(**format_kwargs)
    elif isinstance(object, list):
        one_before_last_object[last_key] = [item.format(**format_kwargs) for item in object]
    else:
        raise ValueError('Unsupported type for object: {}'.format(object))


@transforms.add
def add_artifacts(config, tasks):
    timestamp = _get_timestamp(config)
    version = get_version()

    for task in tasks:
        artifact_template = task.pop("artifact-template", {})
        task["attributes"]["artifacts"] = artifacts = {}

        if task.pop("expose-artifacts", False):
            component = task["attributes"]["component"]
            build_artifact_definitions = task.setdefault("worker", {}).setdefault("artifacts", [])

            all_extensions = get_extensions(component)
            artifact_file_names_per_extension = {
                extension: '{component}-{version}{timestamp}{extension}'.format(
                    component=component,
                    version=version,
                    timestamp='-' + timestamp if task["attributes"]["build-type"] == "snapshot" else '',
                    extension=extension,
                )
                for extension in all_extensions
            }

            for extension, artifact_file_name in artifact_file_names_per_extension.iteritems():
                artifact_full_name = artifact_template["name"].format(
                    artifact_file_name=artifact_file_name,
                )
                build_artifact_definitions.append({
                    "type": artifact_template["type"],
                    "name": artifact_full_name,
                    "path": artifact_template["path"].format(
                        component_path=get_path(component),
                        component=component,
                        version_with_snapshot="{}{}".format(
                            version,
                            "-SNAPSHOT" if task["attributes"]["build-type"] == "snapshot" else ''
                        ),
                        artifact_file_name=artifact_file_name,
                    ),
                })

                artifacts[extension] = artifact_full_name

        yield task
