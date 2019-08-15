# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import (
    optionally_keyed_by,
    resolve_keyed_by,
)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.platforms import archive_format, architecture
from taskgraph.util.workertypes import worker_type_implementation
from taskgraph.transforms.job import job_description_schema
from voluptuous import Required, Optional, Extra


packaging_description_schema = schema.extend({
    # depname is used in taskref's to identify the taskID of the signed things
    Required('depname', default='build'): basestring,

    # unique label to describe this repackaging task
    Optional('label'): basestring,

    Optional('worker-type'): basestring,
    Optional('worker'): object,

    # treeherder is allowed here to override any defaults we use for repackaging.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): job_description_schema['treeherder'],

    # If a l10n task, the corresponding locale
    Optional('locale'): basestring,

    # Routes specific to this task, if defined
    Optional('routes'): [basestring],

    # passed through directly to the job description
    Optional('extra'): job_description_schema['extra'],

    # passed through to job description
    Optional('fetches'): job_description_schema['fetches'],

    # Shipping product and phase
    Optional('shipping-product'): job_description_schema['shipping-product'],
    Optional('shipping-phase'): job_description_schema['shipping-phase'],

    Required('package-formats'): optionally_keyed_by(
        'build-platform', 'release-type', [basestring]),

    # All l10n jobs use mozharness
    Required('mozharness'): {
        Extra: object,
        # Config files passed to the mozharness script
        Required('config'): optionally_keyed_by('build-platform', [basestring]),

        # Additional paths to look for mozharness configs in. These should be
        # relative to the base of the source checkout
        Optional('config-paths'): [basestring],

        # if true, perform a checkout of a comm-central based branch inside the
        # gecko checkout
        Required('comm-checkout', default=False): bool,
    }
})

# The configuration passed to the mozharness repackage script. This defines the
# arguments passed to `mach repackage`
# - `args` is interpolated by mozharness (`{package-name}`, `{installer-tag}`,
#   `{stub-installer-tag}`, `{sfx-stub}`, `{wsx-stub}`, `{fetch-dir}`), with values
#    from mozharness.
# - `inputs` are passed as long-options, with the filename prefixed by
#   `MOZ_FETCH_DIR`. The filename is interpolated by taskgraph
#   (`{archive_format}`).
# - `output` is passed to `--output`, with the filename prefixed by the output
#   directory.
PACKAGE_FORMATS = {
    'mar': {
        'args': [
            'mar',
            '--arch', '{architecture}',
            '--mar-channel-id', '{mar-channel-id}',
        ],
        'inputs': {
            'input': 'target{archive_format}',
            'mar': 'mar-tools/mar',
        },
        'output': "target.complete.mar",
    },
    'mar-bz2': {
        'args': [
            'mar', "--format", "bz2",
            '--arch', '{architecture}',
            '--mar-channel-id', '{mar-channel-id}',
        ],
        'inputs': {
            'input': 'target{archive_format}',
            'mar': 'mar',
        },
        'output': "target.bz2.complete.mar",
    },
    'msi': {
        'args': ['msi', '--wsx', '{wsx-stub}',
                 '--version', '{version_display}',
                 '--locale', '{_locale}',
                 '--arch', '{architecture}',
                 '--candle', '{fetch-dir}/candle.exe',
                 '--light', '{fetch-dir}/light.exe'],
        'inputs': {
            'setupexe': 'target.installer.exe',
        },
        'output': 'target.installer.msi',
    },
    'dmg': {
        'args': ['dmg'],
        'inputs': {
            'input': 'target{archive_format}',
        },
        'output': "target.dmg",
    },
    'installer': {
        'args': [
            "installer",
            "--package-name", "{package-name}",
            "--tag", "{installer-tag}",
            "--sfx-stub", "{sfx-stub}",
        ],
        'inputs': {
            'package': 'target{archive_format}',
            "setupexe": "setup.exe",
        },
        'output': "target.installer.exe",
    },
    'installer-stub': {
        'args': [
            "installer",
            "--tag", "{stub-installer-tag}",
            "--sfx-stub", "{sfx-stub}",
        ],
        'inputs': {
            "setupexe": "setup-stub.exe",
        },
        'output': 'target.stub-installer.exe',
    },
}
MOZHARNESS_EXPANSIONS = [
    'package-name', 'installer-tag', 'fetch-dir',
    'stub-installer-tag', 'sfx-stub', 'wsx-stub',
]

transforms = TransformSequence()
transforms.add_validate(packaging_description_schema)


@transforms.add
def copy_in_useful_magic(config, jobs):
    """Copy attributes from upstream task to be used for keyed configuration."""
    for job in jobs:
        dep = job['primary-dependency']
        job['build-platform'] = dep.attributes.get("build_platform")
        yield job


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        'mozharness.config',
        'package-formats',
    ]
    for job in jobs:
        job = copy.deepcopy(job)  # don't overwrite dict values here
        for field in fields:
            resolve_keyed_by(
                item=job, field=field,
                item_name="?",
                **{
                    'release-type': config.params['release_type'],
                }
            )
        yield job


@transforms.add
def make_repackage_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']

        label = job.get('label',
                        dep_job.label.replace("signing-", "repackage-"))
        job['label'] = label

        yield job


@transforms.add
def make_job_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        dependencies = {dep_job.attributes.get('kind'): dep_job.label}
        if len(dep_job.dependencies) > 1 and not config.kind == 'repackage-msi':
            # repackage-signing can end up with multiple deps...
            raise NotImplementedError(
                "Can't repackage a signing task with multiple dependencies")

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['repackage_type'] = 'repackage'

        locale = attributes.get('locale', job.get('locale'))
        if locale:
            attributes['locale'] = locale

        treeherder = job.get('treeherder', {})
        if attributes.get('nightly'):
            treeherder.setdefault('symbol', 'Nr')
        else:
            treeherder.setdefault('symbol', 'Rpk')
        dep_th_platform = dep_job.task.get('extra', {}).get('treeherder-platform')
        treeherder.setdefault('platform', dep_th_platform)
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        if config.kind == 'repackage-msi':
            treeherder['symbol'] = 'MSI({})'.format(locale or 'N')

        signing_task = None
        repackage_signing_task = None
        for dependency in dependencies.keys():
            if 'repackage-signing' in dependency:
                repackage_signing_task = dependency
            elif 'signing' in dependency:
                signing_task = dependency

        _fetch_subst_locale = 'en-US'
        if locale:
            _fetch_subst_locale = locale

        worker_type = job['worker-type']
        build_platform = attributes['build_platform']

        use_stub = attributes.get('stub-installer')

        repackage_config = []
        package_formats = job.get('package-formats')
        if use_stub and not repackage_signing_task:
            # if repackage_signing_task doesn't exists, generate the stub installer
            package_formats += ['installer-stub']
        for format in package_formats:
            command = copy.deepcopy(PACKAGE_FORMATS[format])
            substs = {
                'archive_format': archive_format(build_platform),
                '_locale': _fetch_subst_locale,
                'architecture': architecture(build_platform),
                'version_display': config.params['version'],
                'mar-channel-id': attributes['mar-channel-id'],
            }
            # Allow us to replace args a well, but specifying things expanded in mozharness
            # Without breaking .format and without allowing unknown through
            substs.update({name: '{{{}}}'.format(name)
                           for name in MOZHARNESS_EXPANSIONS})
            command['inputs'] = {
                name: filename.format(**substs)
                for name, filename in command['inputs'].items()
            }
            command['args'] = [
                arg.format(**substs) for arg in command['args']
            ]
            if 'installer' in format and 'aarch64' not in build_platform:
                command['args'].append('--use-upx')
            repackage_config.append(command)

        run = job.get('mozharness', {})
        run.update({
            'using': 'mozharness',
            'script': 'mozharness/scripts/repackage.py',
            'job-script': 'taskcluster/scripts/builder/repackage.sh',
            'actions': ['setup', 'repackage'],
            'extra-workspace-cache-key': 'repackage',
            'extra-config': {
                'repackage_config': repackage_config,
            },
        })

        worker = job.get('worker', {})
        worker.update({
            'chain-of-trust': True,
            'max-run-time': 7200 if build_platform.startswith('win') else 3600,
            # Don't add generic artifact directory.
            'skip-artifacts': True,
        })

        if locale:
            # Make sure we specify the locale-specific upload dir
            worker.setdefault('env', {}).update(LOCALE=locale)

        worker['artifacts'] = _generate_task_output_files(
            dep_job, worker_type_implementation(config.graph_config, worker_type),
            repackage_config=repackage_config,
            locale=locale,
        )

        description = (
            "Repackaging for locale '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=attributes.get('locale', 'en-US'),
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        task = {
            'label': job['label'],
            'description': description,
            'worker-type': worker_type,
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'routes': job.get('routes', []),
            'extra': job.get('extra', {}),
            'worker': worker,
            'run': run,
            'fetches': _generate_download_config(dep_job, build_platform,
                                                 signing_task, repackage_signing_task,
                                                 locale=locale,
                                                 project=config.params["project"],
                                                 existing_fetch=job.get('fetches')),
            'release-artifacts': [artifact['name'] for artifact in worker['artifacts']]
        }

        if build_platform.startswith('macosx'):
            task.setdefault('fetches', {}).setdefault('toolchain', []).extend([
                'linux64-libdmg',
                'linux64-hfsplus',
                'linux64-node',
            ])
        yield task


def _generate_download_config(task, build_platform, signing_task,
                              repackage_signing_task, locale=None, project=None,
                              existing_fetch=None):
    locale_path = '{}/'.format(locale) if locale else ''
    fetch = {}
    if existing_fetch:
        fetch.update(existing_fetch)

    if repackage_signing_task and build_platform.startswith('win'):
        fetch.update({
            repackage_signing_task: [
                '{}target.installer.exe'.format(locale_path)
            ],
        })
    elif build_platform.startswith('linux') or build_platform.startswith('macosx'):
        fetch.update({
            signing_task: [
                {
                    'artifact': '{}target{}'.format(locale_path, archive_format(build_platform)),
                    'extract': False,
                },
            ],
        })
    elif build_platform.startswith('win'):
        fetch.update({
            signing_task: [
                {
                    'artifact': '{}target.zip'.format(locale_path),
                    'extract': False,
                },
                '{}setup.exe'.format(locale_path),
            ],
        })

        use_stub = task.attributes.get('stub-installer')
        if use_stub:
            fetch[signing_task].append('{}setup-stub.exe'.format(locale_path))

    if fetch:
        return fetch

    raise NotImplementedError('Unsupported build_platform: "{}"'.format(build_platform))


def _generate_task_output_files(task, worker_implementation, repackage_config, locale=None):
    locale_output_path = '{}/'.format(locale) if locale else ''
    artifact_prefix = get_artifact_prefix(task)

    if worker_implementation == ('docker-worker', 'linux'):
        local_prefix = '/builds/worker/workspace/'
    elif worker_implementation == ('generic-worker', 'windows'):
        local_prefix = ''
    else:
        raise NotImplementedError(
            'Unsupported worker implementation: "{}"'.format(worker_implementation))

    output_files = []
    for config in repackage_config:
        output_files.append({
            'type': 'file',
            'path': '{}build/outputs/{}{}'
                    .format(local_prefix, locale_output_path, config['output']),
            'name': '{}/{}{}'.format(artifact_prefix, locale_output_path, config['output']),
        })
    return output_files
