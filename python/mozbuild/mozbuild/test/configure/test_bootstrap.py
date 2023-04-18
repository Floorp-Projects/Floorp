# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozunit import main

from common import BaseConfigureTest


class TestBootstrap(BaseConfigureTest):
    def test_bootstrap(self):
        def get_value_for(arg):
            sandbox = self.get_sandbox({}, {}, [arg], {})
            return sandbox._value_for(sandbox["enable_bootstrap"])

        self.assertEqual(None, get_value_for("--disable-bootstrap"))

        # With `--enable-bootstrap`, anything is bootstrappable
        bootstrap = get_value_for("--enable-bootstrap")
        self.assertTrue(bootstrap("foo"))
        self.assertTrue(bootstrap("bar"))

        # With `--enable-bootstrap=foo,bar`, only foo and bar are bootstrappable
        bootstrap = get_value_for("--enable-bootstrap=foo,bar")
        self.assertTrue(bootstrap("foo"))
        self.assertTrue(bootstrap("bar"))
        self.assertFalse(bootstrap("qux"))

        # With `--enable-bootstrap=-foo`, anything is bootstrappable, except foo
        bootstrap = get_value_for("--enable-bootstrap=-foo")
        self.assertFalse(bootstrap("foo"))
        self.assertTrue(bootstrap("bar"))
        self.assertTrue(bootstrap("qux"))

        # Corner case.
        bootstrap = get_value_for("--enable-bootstrap=-foo,foo,bar")
        self.assertFalse(bootstrap("foo"))
        self.assertTrue(bootstrap("bar"))
        self.assertFalse(bootstrap("qux"))


if __name__ == "__main__":
    main()
