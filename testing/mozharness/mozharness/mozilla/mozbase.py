import os
from mozharness.base.script import PreScriptAction


class MozbaseMixin(object):
    """Automatically set virtualenv requirements to use mozbase
       from test package.
    """
    def __init__(self, *args, **kwargs):
        super(MozbaseMixin, self).__init__(*args, **kwargs)

    @PreScriptAction('create-virtualenv')
    def _install_mozbase(self, action):
        dirs = self.query_abs_dirs()

        requirements = os.path.join(dirs['abs_test_install_dir'],
                                    'config',
                                    'mozbase_requirements.txt')
        if os.path.isfile(requirements):
            self.register_virtualenv_module(requirements=[requirements],
                                            two_pass=True)
            return

        # XXX Bug 879765: Dependent modules need to be listed before parent
        # modules, otherwise they will get installed from the pypi server.
        # XXX Bug 908356: This block can be removed as soon as the
        # in-tree requirements files propagate to all active trees.
        mozbase_dir = os.path.join('tests', 'mozbase')
        self.register_virtualenv_module(
            'manifestparser',
            url=os.path.join(mozbase_dir, 'manifestdestiny')
        )

        for m in ('mozfile', 'mozlog', 'mozinfo', 'moznetwork', 'mozhttpd',
                  'mozcrash', 'mozinstall', 'mozdevice', 'mozprofile',
                  'mozprocess', 'mozrunner'):
            self.register_virtualenv_module(
                m, url=os.path.join(mozbase_dir, m)
            )
