# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest
from shutil import rmtree
from tempfile import mkdtemp

from mozfile.mozfile import NamedTemporaryFile
from mozunit import main

from mozbuild.mozconfig import MozconfigLoader, MozconfigLoadException


class TestMozconfigLoader(unittest.TestCase):
    def setUp(self):
        self._old_env = dict(os.environ)
        os.environ.pop("MOZCONFIG", None)
        os.environ.pop("MOZ_OBJDIR", None)
        os.environ.pop("CC", None)
        os.environ.pop("CXX", None)
        self._temp_dirs = set()

    def tearDown(self):
        os.environ.clear()
        os.environ.update(self._old_env)

        for d in self._temp_dirs:
            rmtree(d)

    def get_loader(self):
        return MozconfigLoader(self.get_temp_dir())

    def get_temp_dir(self):
        d = mkdtemp()
        self._temp_dirs.add(d)

        return d

    def test_read_no_mozconfig(self):
        # This is basically to ensure changes to defaults incur a test failure.
        result = self.get_loader().read_mozconfig()

        self.assertEqual(
            result,
            {
                "path": None,
                "topobjdir": None,
                "configure_args": None,
                "make_flags": None,
                "make_extra": None,
                "env": None,
                "vars": None,
            },
        )

    def test_read_empty_mozconfig(self):
        with NamedTemporaryFile(mode="w") as mozconfig:
            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result["path"], mozconfig.name)
            self.assertIsNone(result["topobjdir"])
            self.assertEqual(result["configure_args"], [])
            self.assertEqual(result["make_flags"], [])
            self.assertEqual(result["make_extra"], [])

            for f in ("added", "removed", "modified"):
                self.assertEqual(len(result["vars"][f]), 0)
                self.assertEqual(len(result["env"][f]), 0)

            self.assertEqual(result["env"]["unmodified"], {})

    def test_read_capture_ac_options(self):
        """Ensures ac_add_options calls are captured."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("ac_add_options --enable-debug\n")
            mozconfig.write("ac_add_options --disable-tests --enable-foo\n")
            mozconfig.write('ac_add_options --foo="bar baz"\n')
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)
            self.assertEqual(
                result["configure_args"],
                ["--enable-debug", "--disable-tests", "--enable-foo", "--foo=bar baz"],
            )

    def test_read_ac_options_substitution(self):
        """Ensure ac_add_options values are substituted."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("ac_add_options --foo=@TOPSRCDIR@\n")
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name)
            self.assertEqual(result["configure_args"], ["--foo=%s" % loader.topsrcdir])

    def test_read_capture_mk_options(self):
        """Ensures mk_add_options calls are captured."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("mk_add_options MOZ_OBJDIR=/foo/bar\n")
            mozconfig.write('mk_add_options MOZ_MAKE_FLAGS="-j8 -s"\n')
            mozconfig.write('mk_add_options FOO="BAR BAZ"\n')
            mozconfig.write("mk_add_options BIZ=1\n")
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)
            self.assertEqual(result["topobjdir"], "/foo/bar")
            self.assertEqual(result["make_flags"], ["-j8", "-s"])
            self.assertEqual(result["make_extra"], ["FOO=BAR BAZ", "BIZ=1"])

    def test_read_no_mozconfig_objdir_environ(self):
        os.environ["MOZ_OBJDIR"] = "obj-firefox"
        result = self.get_loader().read_mozconfig()
        self.assertEqual(result["topobjdir"], "obj-firefox")

    def test_read_empty_mozconfig_objdir_environ(self):
        os.environ["MOZ_OBJDIR"] = "obj-firefox"
        with NamedTemporaryFile(mode="w") as mozconfig:
            result = self.get_loader().read_mozconfig(mozconfig.name)
            self.assertEqual(result["topobjdir"], "obj-firefox")

    def test_read_capture_mk_options_objdir_environ(self):
        """Ensures mk_add_options calls are captured and override the environ."""
        os.environ["MOZ_OBJDIR"] = "obj-firefox"
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("mk_add_options MOZ_OBJDIR=/foo/bar\n")
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)
            self.assertEqual(result["topobjdir"], "/foo/bar")

    def test_read_moz_objdir_substitution(self):
        """Ensure @TOPSRCDIR@ substitution is recognized in MOZ_OBJDIR."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/some-objdir")
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name)

            self.assertEqual(result["topobjdir"], "%s/some-objdir" % loader.topsrcdir)

    def test_read_new_variables(self):
        """New variables declared in mozconfig file are detected."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("CC=/usr/local/bin/clang\n")
            mozconfig.write("CXX=/usr/local/bin/clang++\n")
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(
                result["vars"]["added"],
                {"CC": "/usr/local/bin/clang", "CXX": "/usr/local/bin/clang++"},
            )
            self.assertEqual(result["env"]["added"], {})

    def test_read_exported_variables(self):
        """Exported variables are caught as new variables."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("export MY_EXPORTED=woot\n")
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result["vars"]["added"], {})
            self.assertEqual(result["env"]["added"], {"MY_EXPORTED": "woot"})

    def test_read_modify_variables(self):
        """Variables modified by mozconfig are detected."""
        old_path = os.path.realpath("/usr/bin/gcc")
        new_path = os.path.realpath("/usr/local/bin/clang")
        os.environ["CC"] = old_path

        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write('CC="%s"\n' % new_path)
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result["vars"]["modified"], {})
            self.assertEqual(result["env"]["modified"], {"CC": (old_path, new_path)})

    def test_read_unmodified_variables(self):
        """Variables modified by mozconfig are detected."""
        cc_path = os.path.realpath("/usr/bin/gcc")
        os.environ["CC"] = cc_path

        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result["vars"]["unmodified"], {})
            self.assertEqual(result["env"]["unmodified"], {"CC": cc_path})

    def test_read_removed_variables(self):
        """Variables unset by the mozconfig are detected."""
        cc_path = os.path.realpath("/usr/bin/clang")
        os.environ["CC"] = cc_path

        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("unset CC\n")
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(result["vars"]["removed"], {})
            self.assertEqual(result["env"]["removed"], {"CC": cc_path})

    def test_read_multiline_variables(self):
        """Ensure multi-line variables are captured properly."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write('multi="foo\nbar"\n')
            mozconfig.write("single=1\n")
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(
                result["vars"]["added"], {"multi": "foo\nbar", "single": "1"}
            )
            self.assertEqual(result["env"]["added"], {})

    def test_read_topsrcdir_defined(self):
        """Ensure $topsrcdir references work as expected."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("TEST=$topsrcdir")
            mozconfig.flush()

            loader = self.get_loader()
            result = loader.read_mozconfig(mozconfig.name)

            self.assertEqual(
                result["vars"]["added"]["TEST"], loader.topsrcdir.replace(os.sep, "/")
            )
            self.assertEqual(result["env"]["added"], {})

    def test_read_empty_variable_value(self):
        """Ensure empty variable values are parsed properly."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write("EMPTY=\n")
            mozconfig.write("export EXPORT_EMPTY=\n")
            mozconfig.flush()

            result = self.get_loader().read_mozconfig(mozconfig.name)

            self.assertEqual(
                result["vars"]["added"],
                {
                    "EMPTY": "",
                },
            )
            self.assertEqual(result["env"]["added"], {"EXPORT_EMPTY": ""})

    def test_read_load_exception(self):
        """Ensure non-0 exit codes in mozconfigs are handled properly."""
        with NamedTemporaryFile(mode="w") as mozconfig:
            mozconfig.write('echo "hello world"\n')
            mozconfig.write("exit 1\n")
            mozconfig.flush()

            with self.assertRaises(MozconfigLoadException) as e:
                self.get_loader().read_mozconfig(mozconfig.name)

            self.assertIn(
                "Evaluation of your mozconfig exited with an error", str(e.exception)
            )
            self.assertEqual(e.exception.path, mozconfig.name.replace(os.sep, "/"))
            self.assertEqual(e.exception.output, ["hello world"])


if __name__ == "__main__":
    main()
