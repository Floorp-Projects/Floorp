# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import buildconfig
import os
import unittest
from mozunit import main
from tempfile import mkdtemp
from shutil import rmtree

import mozpack.path as mozpath
from mozbuild.backend.configenvironment import PartialConfigEnvironment

config = {
    'defines': {
        'MOZ_FOO': '1',
        'MOZ_BAR': '2',
    },
    'substs': {
        'MOZ_SUBST_1': '1',
        'MOZ_SUBST_2': '2',
        'CPP': 'cpp',
    },
}


class TestPartial(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

    def _objdir(self):
        objdir = mkdtemp(dir=buildconfig.topsrcdir)
        self.addCleanup(rmtree, objdir)
        return objdir

    def test_auto_substs(self):
        '''Test the automatically set values of ACDEFINES, and ALLDEFINES
        '''
        env = PartialConfigEnvironment(self._objdir())
        env.write_vars(config)
        self.assertEqual(env.substs['ACDEFINES'], '-DMOZ_BAR=2 -DMOZ_FOO=1')
        self.assertEqual(env.defines['ALLDEFINES'], {
            'MOZ_BAR': '2',
            'MOZ_FOO': '1',
        })

    def test_remove_subst(self):
        '''Test removing a subst from the config. The file should be overwritten with 'None'
        '''
        env = PartialConfigEnvironment(self._objdir())
        path = mozpath.join(env.topobjdir, 'config.statusd', 'substs', 'MYSUBST')
        myconfig = config.copy()
        env.write_vars(myconfig)
        with self.assertRaises(KeyError):
            _ = env.substs['MYSUBST']
        self.assertFalse(os.path.exists(path))

        myconfig['substs']['MYSUBST'] = 'new'
        env.write_vars(myconfig)

        self.assertEqual(env.substs['MYSUBST'], 'new')
        self.assertTrue(os.path.exists(path))

        del myconfig['substs']['MYSUBST']
        env.write_vars(myconfig)
        with self.assertRaises(KeyError):
            _ = env.substs['MYSUBST']
        # Now that the subst is gone, the file still needs to be present so that
        # make can update dependencies correctly. Overwriting the file with
        # 'None' is the same as deleting it as far as the
        # PartialConfigEnvironment is concerned, but make can't track a
        # dependency on a file that doesn't exist.
        self.assertTrue(os.path.exists(path))

    def _assert_deps(self, env, deps):
        deps = sorted(['$(wildcard %s)' %
                       (mozpath.join(env.topobjdir, 'config.statusd', d)) for d in deps])
        self.assertEqual(sorted(env.get_dependencies()), deps)

    def test_dependencies(self):
        '''Test getting dependencies on defines and substs.
        '''
        env = PartialConfigEnvironment(self._objdir())
        env.write_vars(config)
        self._assert_deps(env, [])

        self.assertEqual(env.defines['MOZ_FOO'], '1')
        self._assert_deps(env, ['defines/MOZ_FOO'])

        self.assertEqual(env.defines['MOZ_BAR'], '2')
        self._assert_deps(env, ['defines/MOZ_FOO', 'defines/MOZ_BAR'])

        # Getting a define again shouldn't add a redundant dependency
        self.assertEqual(env.defines['MOZ_FOO'], '1')
        self._assert_deps(env, ['defines/MOZ_FOO', 'defines/MOZ_BAR'])

        self.assertEqual(env.substs['MOZ_SUBST_1'], '1')
        self._assert_deps(env, ['defines/MOZ_FOO', 'defines/MOZ_BAR', 'substs/MOZ_SUBST_1'])

        with self.assertRaises(KeyError):
            _ = env.substs['NON_EXISTENT']
        self._assert_deps(env, ['defines/MOZ_FOO', 'defines/MOZ_BAR',
                                'substs/MOZ_SUBST_1', 'substs/NON_EXISTENT'])
        self.assertEqual(env.substs.get('NON_EXISTENT'), None)

    def test_set_subst(self):
        '''Test setting a subst
        '''
        env = PartialConfigEnvironment(self._objdir())
        env.write_vars(config)

        self.assertEqual(env.substs['MOZ_SUBST_1'], '1')
        env.substs['MOZ_SUBST_1'] = 'updated'
        self.assertEqual(env.substs['MOZ_SUBST_1'], 'updated')

        # A new environment should pull the result from the file again.
        newenv = PartialConfigEnvironment(env.topobjdir)
        self.assertEqual(newenv.substs['MOZ_SUBST_1'], '1')

    def test_env_override(self):
        '''Test overriding a subst with an environment variable
        '''
        env = PartialConfigEnvironment(self._objdir())
        env.write_vars(config)

        self.assertEqual(env.substs['MOZ_SUBST_1'], '1')
        self.assertEqual(env.substs['CPP'], 'cpp')

        # Reset the environment and set some environment variables.
        env = PartialConfigEnvironment(env.topobjdir)
        os.environ['MOZ_SUBST_1'] = 'subst 1 environ'
        os.environ['CPP'] = 'cpp environ'

        # The MOZ_SUBST_1 should be overridden by the environment, while CPP is
        # a special variable and should not.
        self.assertEqual(env.substs['MOZ_SUBST_1'], 'subst 1 environ')
        self.assertEqual(env.substs['CPP'], 'cpp')

    def test_update(self):
        '''Test calling update on the substs or defines pseudo dicts
        '''
        env = PartialConfigEnvironment(self._objdir())
        env.write_vars(config)

        mysubsts = {'NEW': 'new'}
        mysubsts.update(env.substs.iteritems())
        self.assertEqual(mysubsts['NEW'], 'new')
        self.assertEqual(mysubsts['CPP'], 'cpp')

        mydefines = {'DEBUG': '1'}
        mydefines.update(env.defines.iteritems())
        self.assertEqual(mydefines['DEBUG'], '1')
        self.assertEqual(mydefines['MOZ_FOO'], '1')


if __name__ == "__main__":
    main()
