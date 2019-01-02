# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Decision task for pull requests and pushes
"""

from __future__ import print_function
import datetime
import os
import taskcluster
import sys

import lib.module_definitions
import lib.tasks


TASK_ID = os.environ.get('TASK_ID')
REPO_URL = os.environ.get('MOBILE_HEAD_REPOSITORY')
BRANCH = os.environ.get('MOBILE_HEAD_BRANCH')
COMMIT = os.environ.get('MOBILE_HEAD_REV')
PR_TITLE = os.environ.get('GITHUB_PULL_TITLE', '')

# If we see this text inside a pull request title then we will not execute any tasks for this PR.
SKIP_TASKS_TRIGGER = '[ci skip]'


def create_task(name, description, command, scopes = []):
    return create_raw_task(name, description, "./gradlew --no-daemon clean %s" % command, scopes)


def create_raw_task(name, description, full_command, scopes = []):
    created = datetime.datetime.now()
    expires = taskcluster.fromNow('1 year')
    deadline = taskcluster.fromNow('1 day')

    return {
        "workerType": 'github-worker',
        "taskGroupId": TASK_ID,
        "expires": taskcluster.stringDate(expires),
        "retries": 5,
        "created": taskcluster.stringDate(created),
        "tags": {},
        "priority": "lowest",
        "schedulerId": "taskcluster-github",
        "deadline": taskcluster.stringDate(deadline),
        "dependencies": [ TASK_ID ],
        "routes": [],
        "scopes": scopes,
        "requires": "all-completed",
        "payload": {
            "features": {
                'taskclusterProxy': True
            },
            "maxRunTime": 7200,
            "image": "mozillamobile/android-components:1.11",
            "command": [
                "/bin/bash",
                "--login",
                "-cx",
                "export TERM=dumb && git fetch %s %s && git config advice.detachedHead false && git checkout %s && %s" % (REPO_URL, BRANCH, COMMIT, full_command)
            ],
            "artifacts": {},
            "env": {
                "TASK_GROUP_ID": TASK_ID
            }
        },
        "provisionerId": "aws-provisioner-v1",
        "metadata": {
            "name": name,
            "description": description,
            "owner": "skaspari@mozilla.com",
            "source": "https://github.com/mozilla-mobile/android-components"
        }
    }


def create_module_tasks(module):
    # Helper functions relevant to creating taskcluter gradle module task definitions.
    def taskcluster_task_definition(gradle_task, subtitle=""):
        return create_task(
            name="Android Components - Module %s %s" % (module, subtitle),
            description="Running configured tasks for %s" % module,
            command="-Pcoverage %s && automation/taskcluster/action/upload_coverage_report.sh" % gradle_task,
            scopes = [
                "secrets:get:project/mobile/android-components/public-tokens"
            ])

    def gradle_module_task_name(module, gradle_task_name):
        return "%s:%s" % (module, gradle_task_name)

    def craft_definition(module, variant, run_tests=True, lint_task=None):
        module_task = gradle_module_task_name(module, "assemble%s" % variant)
        subtitle = "assemble"

        if run_tests:
            module_task = "%s %s" % (module_task, gradle_module_task_name(module, "test%sDebugUnitTest" % variant))
            subtitle = "%sAndTest" % subtitle

        scheduled_lint = False
        if lint_task and variant in lint_task:
            module_task = "%s %s" % (module_task, gradle_module_task_name(module, lint_task))
            subtitle = "%sAndLintDebug" % subtitle
            scheduled_lint = True

        return taskcluster_task_definition(module_task, subtitle=subtitle + variant), scheduled_lint

    # Takes list of variants and produces taskcluter task definitions.
    # Schedules a specified lint task at most once.
    def variants_to_definitions(variants, run_tests=True, lint_task=None):
        scheduled_lint = False
        definitions = []
        for variant in variants:
            task_definition, definition_has_lint = craft_definition(
                module, variant, run_tests=run_tests, lint_task=lint_task)
            if definition_has_lint:
                scheduled_lint = True
            lint_task = lint_task if lint_task and not definition_has_lint else None
            definitions.append(task_definition)
        return definitions, scheduled_lint

    # Module settings that describe which task definitions will be created.
    # Absence of a module override means that default definition pattern will be used for the module.
    module_overrides = {
        ":samples-browser": {
            "assembleOnly/Test": (
                ["ServoArm", "SystemUniversal"],
                [
                    "GeckoBetaAarch64", "GeckoBetaArm", "GeckoBetaX86",
                    "GeckoNightlyAarch64", "GeckoNightlyArm", "GeckoNightlyX86",
                    "GeckoReleaseAarch64", "GeckoReleaseArm", "GeckoReleaseX86"
                ]
            ),
            "lintTask": "lintGeckoBetaArmDebug"
        },
        ":support-test": {
            "lintTask": "lint"
        },
        ":tooling-lint": {
            "lintTask": "lint"
        }
    }

    override = module_overrides.get(module, {})
    task_definitions = []

    # If assemble/test variant lists are specified, create individual tasks as appropriate,
    # while also checking if we can sneak in a specified lint task along with a matching assemble
    # or test variant. If lint task isn't specified, it's created separately.
    if "assembleOnly/Test" in override:
        assemble_only, assemble_and_test = override["assembleOnly/Test"]

        # As an optimization, lint task will appear at most once in our final list of definitions.
        lint_task = override.get("lintTask", "lintRelease")

        # Assemble-only definitions with an optional lint task (if it matches variants).
        assemble_only_definitions, scheduled_lint_with_assemble = variants_to_definitions(
            assemble_only, run_tests=False, lint_task=lint_task)
        # Assemble-and-test definitions with an optional lint task if it wasn't present
        # above and matches variants.
        assemble_and_test_definitions, scheduled_lint_with_test = variants_to_definitions(
            assemble_and_test, run_tests=True, lint_task=lint_task if lint_task and not scheduled_lint_with_assemble else None)
        # Combine two lists of task definitions.
        task_definitions += assemble_only_definitions + assemble_and_test_definitions

        # If neither list has a lint task within it, append a separate lint task.
        # Overhead of a separate task just for linting is quite significant, but this should be a rare case.
        if not scheduled_lint_with_assemble and not scheduled_lint_with_test:
            task_definitions.append(taskcluster_task_definition(
                gradle_module_task_name(module, lint_task),
                subtitle="onlyLintRelease"))

    # If only the lint task override is specified, 'assemble' and 'test' are left as-is.
    elif "lintTask" in override:
        task_definitions.append(taskcluster_task_definition(
            " ".join([gradle_module_task_name(module, gradle_task) for gradle_task in ['assemble', 'test', override["lintTask"]]]),
            subtitle="assembleAndTestAndCustomLintAll"))

    # Default module task configuration is a single gradle task which runs assemble, test and lint for every
    # variant configured for the given module.
    else:
        task_definitions.append(taskcluster_task_definition(
            " ".join([gradle_module_task_name(module, gradle_task) for gradle_task in ['assemble', 'test', 'lintRelease']]),
            subtitle="assembleAndTestAndLintReleaseAll"))

    return task_definitions

def create_detekt_task():
    return create_task(
        name='Android Components - detekt',
        description='Running detekt over all modules',
        command='detekt')


def create_ktlint_task():
    return create_task(
        name='Android Components - ktlint',
        description='Running ktlint over all modules',
        command='ktlint')


def create_compare_locales_task():
    return create_raw_task(
        name='Android Components - compare-locales',
        description='Validate strings.xml with compare-locales',
        full_command='pip install "compare-locales>=5.0.2,<6.0" && compare-locales --validate l10n.toml .')


if __name__ == "__main__":
    if SKIP_TASKS_TRIGGER in PR_TITLE:
        print("Pull request title contains", SKIP_TASKS_TRIGGER)
        print("Exit")
        exit(0)

    queue = taskcluster.Queue({ 'baseUrl': 'http://taskcluster/queue/v1' })

    modules = [':' + artifact['name'] for artifact in lib.module_definitions.from_gradle()]

    if len(modules) == 0:
        print("Could not get module names from gradle")
        sys.exit(2)

    for module in modules:
        tasks = create_module_tasks(module)
        for task in tasks:
            task_id = taskcluster.slugId()
            lib.tasks.schedule_task(queue, task_id, task)

    lib.tasks.schedule_task(queue, taskcluster.slugId(), create_detekt_task())
    lib.tasks.schedule_task(queue, taskcluster.slugId(), create_ktlint_task())
    lib.tasks.schedule_task(queue, taskcluster.slugId(), create_compare_locales_task())
