from __future__ import absolute_import, print_function
import codecs
from difflib import unified_diff
import logging
import os
import re
import shutil
import sys

import hglib
from mozboot.util import get_state_dir
import mozpack.path as mozpath

from compare_locales.merge import merge_channels
from compare_locales.paths.files import ProjectFiles
from compare_locales.paths.configparser import TOMLParser
from fluent.migrate import validator
from fluent.syntax import FluentParser, FluentSerializer


def inspect_migration(path):
    '''Validate recipe and extract some metadata.
    '''
    return validator.Validator.validate(path)


def prepare_object_dir(cmd):
    '''Prepare object dir to have an up-to-date clone of gecko-strings.

    We run this once per mach invocation, for all tested migrations.
    '''
    obj_dir = mozpath.join(cmd.topobjdir, 'python', 'l10n')
    if not os.path.exists(obj_dir):
        os.makedirs(obj_dir)
    state_dir = get_state_dir()
    if os.path.exists(mozpath.join(state_dir, 'gecko-strings')):
        cmd.run_process(
            ['hg', 'pull', '-u'],
            cwd=mozpath.join(state_dir, 'gecko-strings')
        )
    else:
        cmd.run_process(
            ['hg', 'clone', 'https://hg.mozilla.org/l10n/gecko-strings'],
            cwd=state_dir,
        )
    return obj_dir


def diff_resources(left_path, right_path):
    parser = FluentParser(with_spans=False)
    serializer = FluentSerializer(with_junk=True)
    lines = []
    for p in (left_path, right_path):
        with codecs.open(p, encoding='utf-8') as fh:
            res = parser.parse(fh.read())
            lines.append(serializer.serialize(res).splitlines(True))
    sys.stdout.writelines(
        chunk for chunk in unified_diff(lines[0], lines[1], left_path, right_path)
    )


def test_migration(cmd, obj_dir, to_test, references):
    '''Test the given recipe.

    This creates a workdir by l10n-merging gecko-strings and the m-c source,
    to mimmic gecko-strings after the patch to test landed.
    It then runs the recipe with a gecko-strings clone as localization, both
    dry and wet.
    It inspects the generated commits, and shows a diff between the merged
    reference and the generated content.
    The diff is intended to be visually inspected. Some changes might be
    expected, in particular when formatting of the en-US strings is different.
    '''
    rv = 0
    migration_name = os.path.splitext(os.path.split(to_test)[1])[0]
    work_dir = mozpath.join(obj_dir, migration_name)

    paths = os.path.normpath(to_test).split(os.sep)
    # Migration modules should be in a sub-folder of l10n.
    migration_module = '.'.join(paths[paths.index('l10n')+1:-1]) + '.' + migration_name

    if os.path.exists(work_dir):
        shutil.rmtree(work_dir)
    os.makedirs(mozpath.join(work_dir, 'reference'))
    l10n_toml = mozpath.join(cmd.topsrcdir, cmd.substs['MOZ_BUILD_APP'], 'locales', 'l10n.toml')
    pc = TOMLParser().parse(l10n_toml, env={
        'l10n_base': work_dir
    })
    pc.set_locales(['reference'])
    files = ProjectFiles('reference', [pc])
    for ref in references:
        if ref != mozpath.normpath(ref):
            cmd.log(logging.ERROR, 'fluent-migration-test', {
                'file': to_test,
                'ref': ref,
            }, 'Reference path "{ref}" needs to be normalized for {file}')
            rv = 1
            continue
        full_ref = mozpath.join(work_dir, 'reference', ref)
        m = files.match(full_ref)
        if m is None:
            raise ValueError("Bad reference path: " + ref)
        m_c_path = m[1]
        g_s_path = mozpath.join(work_dir, 'gecko-strings', ref)
        resources = [
            b'' if not os.path.exists(f)
            else open(f, 'rb').read()
            for f in (g_s_path, m_c_path)
        ]
        ref_dir = os.path.dirname(full_ref)
        if not os.path.exists(ref_dir):
            os.makedirs(ref_dir)
        open(full_ref, 'wb').write(merge_channels(ref, resources))
    client = hglib.clone(
        source=mozpath.join(get_state_dir(), 'gecko-strings'),
        dest=mozpath.join(work_dir, 'en-US')
    )
    client.open()
    old_tip = client.tip().node
    run_migration = [
        cmd._virtualenv_manager.python_path,
        '-m', 'fluent.migrate.tool',
        '--lang', 'en-US',
        '--reference-dir', mozpath.join(work_dir, 'reference'),
        '--localization-dir', mozpath.join(work_dir, 'en-US'),
        '--dry-run',
        migration_module
    ]
    cmd.run_process(
        run_migration,
        cwd=work_dir,
        line_handler=print,
    )
    # drop --dry-run
    run_migration.pop(-2)
    cmd.run_process(
        run_migration,
        cwd=work_dir,
        line_handler=print,
    )
    tip = client.tip().node
    if old_tip == tip:
        cmd.log(logging.WARN, 'fluent-migration-test', {
            'file': to_test,
        }, 'No migration applied for {file}')
        return rv
    for ref in references:
        diff_resources(
            mozpath.join(work_dir, 'reference', ref),
            mozpath.join(work_dir, 'en-US', ref),
        )
    messages = [l.desc.decode('utf-8') for l in client.log(b'::%s - ::%s' % (tip, old_tip))]
    bug = re.search('[0-9]{5,}', migration_name).group()
    # Just check first message for bug number, they're all following the same pattern
    if bug not in messages[0]:
        rv = 1
        cmd.log(logging.ERROR, 'fluent-migration-test', {
            'file': to_test,
        }, 'Missing or wrong bug number for {file}')
    if any(
        'part {}'.format(n + 1) not in msg
        for n, msg in enumerate(messages)
    ):
        rv = 1
        cmd.log(logging.ERROR, 'fluent-migration-test', {
            'file': to_test,
        }, 'Commit messages should have "part {{index}}" for {file}')
    return rv
