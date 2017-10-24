from __future__ import absolute_import

import os

import mock
import mozunit

from talos.ffsetup import FFSetup


class TestFFSetup(object):

    def setup_method(self, method):
        self.ffsetup = FFSetup(
            {  # browser_config
                "env": {},
                "symbols_path": "",
                "preferences": {},
                "webserver": "",
                "extensions": []
            },
            {  #test_config
                "preferences": {},
                "extensions": [],
                "profile": None
            }
        )

        # setup proxy logger

    def test_clean(self):
        # tmp dir removed
        assert self.ffsetup._tmp_dir is not None
        assert os.path.exists(self.ffsetup._tmp_dir) is True

        self.ffsetup.clean()

        assert self.ffsetup._tmp_dir is not None
        assert os.path.exists(self.ffsetup._tmp_dir) is False

        # gecko profile also cleaned
        gecko_profile = mock.Mock()
        self.ffsetup.gecko_profile = gecko_profile

        self.ffsetup.clean()

        assert gecko_profile.clean.called is True

#     def test_as_context_manager(self):
#         self.ffsetup._init_env = mock.Mock()
#         self.ffsetup._init_profile = mock.Mock()
#         self.ffsetup._run_profile = mock.Mock()
#         self.ffsetup._init_gecko_profile = mock.Mock()
#
#         with self.ffsetup as setup:
#             # env initiated
#             self.assertIsNotNone(setup.env)
#             # profile initiated
#             self.assertTrue(setup._init_profile.called)
#             # gecko profile initiated
#
#         # except raised
#         pass
#
#     def test_environment_init(self):
#         # self.env not empty
#         # browser_config env vars in self.env
#         # multiple calls return same self.env
#         pass
#
#     def test_profile_init(self):
#         # addons get installed
#         # webextensions get installed
#         # preferences contain interpolated values
#         # profile path is added
#         pass
#
#     def test_run_profile(self):
#         # exception raised
#         # browser process launched
#         pass
#
#     def test_gecko_profile_init(self):
#         # complains on not provided upload_dir
#         # self.gecko_profile not None
#         pass


if __name__ == '__main__':
    mozunit.main()
