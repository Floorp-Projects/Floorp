#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
'''Python usage, esp. virtualenv.
'''

import os
import subprocess
import sys
import time
import json
import traceback

from mozharness.base.script import (
    PostScriptAction,
    PostScriptRun,
    PreScriptAction,
    PreScriptRun,
)
from mozharness.base.errors import VirtualenvErrorList
from mozharness.base.log import WARNING, FATAL
from mozharness.mozilla.proxxy import Proxxy

def get_tlsv1_post():
    # Monkeypatch to work around SSL errors in non-bleeding-edge Python.
    # Taken from https://lukasa.co.uk/2013/01/Choosing_SSL_Version_In_Requests/
    import requests
    from requests.packages.urllib3.poolmanager import PoolManager
    import ssl

    class TLSV1Adapter(requests.adapters.HTTPAdapter):
        def init_poolmanager(self, connections, maxsize, block=False):
            self.poolmanager = PoolManager(num_pools=connections,
                                           maxsize=maxsize,
                                           block=block,
                                           ssl_version=ssl.PROTOCOL_TLSv1)
    s = requests.Session()
    s.mount('https://', TLSV1Adapter())
    return s.post

# Virtualenv {{{1
virtualenv_config_options = [
    [["--venv-path", "--virtualenv-path"], {
        "action": "store",
        "dest": "virtualenv_path",
        "default": "venv",
        "help": "Specify the path to the virtualenv top level directory"
    }],
    [["--virtualenv"], {
        "action": "store",
        "dest": "virtualenv",
        "help": "Specify the virtualenv executable to use"
    }],
    [["--find-links"], {
        "action": "extend",
        "dest": "find_links",
        "help": "URL to look for packages at"
    }],
    [["--pip-index"], {
        "action": "store_true",
        "default": True,
        "dest": "pip_index",
        "help": "Use pip indexes (default)"
    }],
    [["--no-pip-index"], {
        "action": "store_false",
        "dest": "pip_index",
        "help": "Don't use pip indexes"
    }],
]


class VirtualenvMixin(object):
    '''BaseScript mixin, designed to create and use virtualenvs.

    Config items:
     * virtualenv_path points to the virtualenv location on disk.
     * virtualenv_modules lists the module names.
     * MODULE_url list points to the module URLs (optional)
    Requires virtualenv to be in PATH.
    Depends on ScriptMixin
    '''
    python_paths = {}
    site_packages_path = None

    def __init__(self, *args, **kwargs):
        self._virtualenv_modules = []
        super(VirtualenvMixin, self).__init__(*args, **kwargs)

    def register_virtualenv_module(self, name=None, url=None, method=None,
                                   requirements=None, optional=False,
                                   two_pass=False, editable=False):
        """Register a module to be installed with the virtualenv.

        This method can be called up until create_virtualenv() to register
        modules that should be installed in the virtualenv.

        See the documentation for install_module for how the arguments are
        applied.
        """
        self._virtualenv_modules.append((name, url, method, requirements,
                                         optional, two_pass, editable))

    def query_virtualenv_path(self):
        c = self.config
        dirs = self.query_abs_dirs()
        virtualenv = None
        if 'abs_virtualenv_dir' in dirs:
            virtualenv = dirs['abs_virtualenv_dir']
        elif c.get('virtualenv_path'):
            if os.path.isabs(c['virtualenv_path']):
                virtualenv = c['virtualenv_path']
            else:
                virtualenv = os.path.join(dirs['abs_work_dir'],
                                          c['virtualenv_path'])
        return virtualenv

    def query_python_path(self, binary="python"):
        """Return the path of a binary inside the virtualenv, if
        c['virtualenv_path'] is set; otherwise return the binary name.
        Otherwise return None
        """
        if binary not in self.python_paths:
            bin_dir = 'bin'
            if self._is_windows():
                bin_dir = 'Scripts'
            virtualenv_path = self.query_virtualenv_path()
            if virtualenv_path:
                self.python_paths[binary] = os.path.abspath(os.path.join(virtualenv_path, bin_dir, binary))
            else:
                self.python_paths[binary] = self.query_exe(binary)
        return self.python_paths[binary]

    def query_python_site_packages_path(self):
        if self.site_packages_path:
            return self.site_packages_path
        python = self.query_python_path()
        self.site_packages_path = self.get_output_from_command(
            [python, '-c',
             'from distutils.sysconfig import get_python_lib; ' +
             'print(get_python_lib())'])
        return self.site_packages_path

    def package_versions(self, pip_freeze_output=None, error_level=WARNING, log_output=False):
        """
        reads packages from `pip freeze` output and returns a dict of
        {package_name: 'version'}
        """
        packages = {}

        if pip_freeze_output is None:
            # get the output from `pip freeze`
            pip = self.query_python_path("pip")
            if not pip:
                self.log("package_versions: Program pip not in path", level=error_level)
                return {}
            pip_freeze_output = self.get_output_from_command([pip, "freeze"], silent=True, ignore_errors=True)
            if not isinstance(pip_freeze_output, basestring):
                self.fatal("package_versions: Error encountered running `pip freeze`: %s" % pip_freeze_output)

        for line in pip_freeze_output.splitlines():
            # parse the output into package, version
            line = line.strip()
            if not line:
                # whitespace
                continue
            if line.startswith('-'):
                # not a package, probably like '-e http://example.com/path#egg=package-dev'
                continue
            if '==' not in line:
                self.fatal("pip_freeze_packages: Unrecognized output line: %s" % line)
            package, version = line.split('==', 1)
            packages[package] = version

        if log_output:
            self.info("Current package versions:")
            for package in sorted(packages):
                self.info("  %s == %s" % (package, packages[package]))

        return packages

    def is_python_package_installed(self, package_name, error_level=WARNING):
        """
        Return whether the package is installed
        """
        packages = self.package_versions(error_level=error_level).keys()
        return package_name.lower() in [package.lower() for package in packages]

    def install_module(self, module=None, module_url=None, install_method=None,
                       requirements=(), optional=False, global_options=[],
                       no_deps=False, editable=False):
        """
        Install module via pip.

        module_url can be a url to a python package tarball, a path to
        a directory containing a setup.py (absolute or relative to work_dir)
        or None, in which case it will default to the module name.

        requirements is a list of pip requirements files.  If specified, these
        will be combined with the module_url (if any), like so:

        pip install -r requirements1.txt -r requirements2.txt module_url
        """
        c = self.config
        dirs = self.query_abs_dirs()
        env = self.query_env()
        venv_path = self.query_virtualenv_path()
        self.info("Installing %s into virtualenv %s" % (module, venv_path))
        if not module_url:
            module_url = module
        if install_method in (None, 'pip'):
            if not module_url and not requirements:
                self.fatal("Must specify module and/or requirements")
            pip = self.query_python_path("pip")
            if c.get("verbose_pip"):
                command = [pip, "-v", "install"]
            else:
                command = [pip, "install"]
            if no_deps:
                command += ["--no-deps"]
            # To avoid timeouts with our pypi server, increase default timeout:
            # https://bugzilla.mozilla.org/show_bug.cgi?id=1007230#c802
            command += ['--timeout', str(c.get('pip_timeout', 120))]
            for requirement in requirements:
                command += ["-r", requirement]
            if c.get('find_links') and not c["pip_index"]:
                command += ['--no-index']
            for opt in global_options:
                command += ["--global-option", opt]
        elif install_method == 'easy_install':
            if not module:
                self.fatal("module parameter required with install_method='easy_install'")
            if requirements:
                # Install pip requirements files separately, since they're
                # not understood by easy_install.
                self.install_module(requirements=requirements,
                                    install_method='pip')
            # Allow easy_install to be overridden by
            # self.config['exes']['easy_install']
            default = 'easy_install'
            if self._is_windows():
                # Don't invoke `easy_install` directly on windows since
                # the 'install' in the executable name hits UAC
                # - http://answers.microsoft.com/en-us/windows/forum/windows_7-security/uac-message-do-you-want-to-allow-the-following/bea30ad8-9ef8-4897-aab4-841a65f7af71
                # - https://bugzilla.mozilla.org/show_bug.cgi?id=791840
                default = [self.query_python_path(), self.query_python_path('easy_install-script.py')]
            command = self.query_exe('easy_install', default=default, return_type="list")
        else:
            self.fatal("install_module() doesn't understand an install_method of %s!" % install_method)

        # Add --find-links pages to look at
        proxxy = Proxxy(self.config, self.log_obj)
        for link in proxxy.get_proxies_and_urls(c.get('find_links', [])):
            command.extend(["--find-links", link])

        # module_url can be None if only specifying requirements files
        if module_url:
            if editable:
                if install_method in (None, 'pip'):
                    command += ['-e']
                else:
                    self.fatal("editable installs not supported for install_method %s" % install_method)
            command += [module_url]

        # If we're only installing a single requirements file, use
        # the file's directory as cwd, so relative paths work correctly.
        cwd = dirs['abs_work_dir']
        if not module and len(requirements) == 1:
            cwd = os.path.dirname(requirements[0])

        quoted_command = subprocess.list2cmdline(command)
        # Allow for errors while building modules, but require a
        # return status of 0.
        self.retry(
            self.run_command,
            # None will cause default value to be used
            attempts=1 if optional else None,
            good_statuses=(0,),
            error_level=WARNING if optional else FATAL,
            error_message='Could not install python package: ' + quoted_command + ' failed after %(attempts)d tries!',
            args=[command, ],
            kwargs={
                'error_list': VirtualenvErrorList,
                'cwd': cwd,
                'env': env,
                # WARNING only since retry will raise final FATAL if all
                # retry attempts are unsuccessful - and we only want
                # an ERROR of FATAL if *no* retry attempt works
                'error_level': WARNING,
            }
        )

    def create_virtualenv(self, modules=(), requirements=()):
        """
        Create a python virtualenv.

        The virtualenv exe can be defined in c['virtualenv'] or
        c['exes']['virtualenv'], as a string (path) or list (path +
        arguments).

        c['virtualenv_python_dll'] is an optional config item that works
        around an old windows virtualenv bug.

        virtualenv_modules can be a list of module names to install, e.g.

            virtualenv_modules = ['module1', 'module2']

        or it can be a heterogeneous list of modules names and dicts that
        define a module by its name, url-or-path, and a list of its global
        options.

            virtualenv_modules = [
                {
                    'name': 'module1',
                    'url': None,
                    'global_options': ['--opt', '--without-gcc']
                },
                {
                    'name': 'module2',
                    'url': 'http://url/to/package',
                    'global_options': ['--use-clang']
                },
                {
                    'name': 'module3',
                    'url': os.path.join('path', 'to', 'setup_py', 'dir')
                    'global_options': []
                },
                'module4'
            ]

        virtualenv_requirements is an optional list of pip requirements files to
        use when invoking pip, e.g.,

            virtualenv_requirements = [
                '/path/to/requirements1.txt',
                '/path/to/requirements2.txt'
            ]
        """
        c = self.config
        dirs = self.query_abs_dirs()
        venv_path = self.query_virtualenv_path()
        self.info("Creating virtualenv %s" % venv_path)
        virtualenv = c.get('virtualenv', self.query_exe('virtualenv'))
        if isinstance(virtualenv, str):
            # allow for [python, virtualenv] in config
            virtualenv = [virtualenv]

        if not os.path.exists(virtualenv[0]) and not self.which(virtualenv[0]):
            self.add_summary("The executable '%s' is not found; not creating "
                             "virtualenv!" % virtualenv[0], level=FATAL)
            return -1

        # https://bugs.launchpad.net/virtualenv/+bug/352844/comments/3
        # https://bugzilla.mozilla.org/show_bug.cgi?id=700415#c50
        if c.get('virtualenv_python_dll'):
            # We may someday want to copy a differently-named dll, but
            # let's not think about that right now =\
            dll_name = os.path.basename(c['virtualenv_python_dll'])
            target = self.query_python_path(dll_name)
            scripts_dir = os.path.dirname(target)
            self.mkdir_p(scripts_dir)
            self.copyfile(c['virtualenv_python_dll'], target, error_level=WARNING)
        else:
            self.mkdir_p(dirs['abs_work_dir'])

        # make this list configurable?
        for module in ('distribute', 'pip'):
            if c.get('%s_url' % module):
                self.download_file(c['%s_url' % module],
                                   parent_dir=dirs['abs_work_dir'])

        virtualenv_options = c.get('virtualenv_options',
                                   ['--no-site-packages', '--distribute'])

        if os.path.exists(self.query_python_path()):
            self.info("Virtualenv %s appears to already exist; skipping virtualenv creation." % self.query_python_path())
        else:
            self.run_command(virtualenv + virtualenv_options + [venv_path],
                             cwd=dirs['abs_work_dir'],
                             error_list=VirtualenvErrorList,
                             halt_on_failure=True)
        if not modules:
            modules = c.get('virtualenv_modules', [])
        if not requirements:
            requirements = c.get('virtualenv_requirements', [])
        if not modules and requirements:
            self.install_module(requirements=requirements,
                                install_method='pip')
        for module in modules:
            module_url = module
            global_options = []
            if isinstance(module, dict):
                if module.get('name', None):
                    module_name = module['name']
                else:
                    self.fatal("Can't install module without module name: %s" %
                               str(module))
                module_url = module.get('url', None)
                global_options = module.get('global_options', [])
            else:
                module_url = self.config.get('%s_url' % module, module_url)
                module_name = module
            install_method = 'pip'
            if module_name in ('pywin32',):
                install_method = 'easy_install'
            self.install_module(module=module_name,
                                module_url=module_url,
                                install_method=install_method,
                                requirements=requirements,
                                global_options=global_options)

        for module, url, method, requirements, optional, two_pass, editable in \
                self._virtualenv_modules:
            if two_pass:
                self.install_module(
                    module=module, module_url=url,
                    install_method=method, requirements=requirements or (),
                    optional=optional, no_deps=True, editable=editable
                )
            self.install_module(
                module=module, module_url=url,
                install_method=method, requirements=requirements or (),
                optional=optional, editable=editable
            )

        self.info("Done creating virtualenv %s." % venv_path)

        self.package_versions(log_output=True)

    def activate_virtualenv(self):
        """Import the virtualenv's packages into this Python interpreter."""
        bin_dir = os.path.dirname(self.query_python_path())
        activate = os.path.join(bin_dir, 'activate_this.py')
        execfile(activate, dict(__file__=activate))


class ResourceMonitoringMixin(object):
    """Provides resource monitoring capabilities to scripts.

    When this class is in the inheritance chain, resource usage stats of the
    executing script will be recorded.

    This class requires the VirtualenvMixin in order to install a package used
    for recording resource usage.

    While we would like to record resource usage for the entirety of a script,
    since we require an external package, we can only record resource usage
    after that package is installed (as part of creating the virtualenv).
    That's just the way things have to be.
    """
    def __init__(self, *args, **kwargs):
        super(ResourceMonitoringMixin, self).__init__(*args, **kwargs)

        self.register_virtualenv_module('psutil>=3.1.1', method='pip',
                                        optional=True)
        self.register_virtualenv_module('mozsystemmonitor==0.1',
                                        method='pip', optional=True)
        self._resource_monitor = None

    @PostScriptAction('create-virtualenv')
    def _start_resource_monitoring(self, action, success=None):
        self.activate_virtualenv()

        # Resource Monitor requires Python 2.7, however it's currently optional.
        # Remove when all machines have had their Python version updated (bug 711299).
        if sys.version_info[:2] < (2, 7):
            self.warning('Resource monitoring will not be enabled! Python 2.7+ required.')
            return

        try:
            from mozsystemmonitor.resourcemonitor import SystemResourceMonitor

            self.info("Starting resource monitoring.")
            self._resource_monitor = SystemResourceMonitor(poll_interval=1.0)
            self._resource_monitor.start()
        except Exception:
            self.warning("Unable to start resource monitor: %s" %
                         traceback.format_exc())

    @PreScriptAction
    def _resource_record_pre_action(self, action):
        # Resource monitor isn't available until after create-virtualenv.
        if not self._resource_monitor:
            return

        self._resource_monitor.begin_phase(action)

    @PostScriptAction
    def _resource_record_post_action(self, action, success=None):
        # Resource monitor isn't available until after create-virtualenv.
        if not self._resource_monitor:
            return

        self._resource_monitor.finish_phase(action)

    @PostScriptRun
    def _resource_record_post_run(self):
        if not self._resource_monitor:
            return

        # This should never raise an exception. This is a workaround until
        # mozsystemmonitor is fixed. See bug 895388.
        try:
            self._resource_monitor.stop()
            self._log_resource_usage()

            # Upload a JSON file containing the raw resource data.
            try:
                upload_dir = self.query_abs_dirs()['abs_blob_upload_dir']
                with open(os.path.join(upload_dir, 'resource-usage.json'), 'wb') as fh:
                    json.dump(self._resource_monitor.as_dict(), fh,
                              sort_keys=True, indent=4)
            except (AttributeError, KeyError):
                self.exception('could not upload resource usage JSON',
                               level=WARNING)

        except Exception:
            self.warning("Exception when reporting resource usage: %s" %
                         traceback.format_exc())

    def _log_resource_usage(self):
        rm = self._resource_monitor

        if rm.start_time is None:
            return

        def resources(phase):
            cpu_percent = rm.aggregate_cpu_percent(phase=phase, per_cpu=False)
            cpu_times = rm.aggregate_cpu_times(phase=phase, per_cpu=False)
            io = rm.aggregate_io(phase=phase)

            swap_in = sum(m.swap.sin for m in rm.measurements)
            swap_out = sum(m.swap.sout for m in rm.measurements)

            return cpu_percent, cpu_times, io, (swap_in, swap_out)

        def log_usage(prefix, duration, cpu_percent, cpu_times, io):
            message = '{prefix} - Wall time: {duration:.0f}s; ' \
                'CPU: {cpu_percent}; ' \
                'Read bytes: {io_read_bytes}; Write bytes: {io_write_bytes}; ' \
                'Read time: {io_read_time}; Write time: {io_write_time}'

            # XXX Some test harnesses are complaining about a string being
            # being fed into a 'f' formatter. This will help diagnose the
            # issue.
            cpu_percent_str = str(round(cpu_percent)) + '%' if cpu_percent else "Can't collect data"

            try:
                self.info(
                    message.format(
                        prefix=prefix, duration=duration,
                        cpu_percent=cpu_percent_str, io_read_bytes=io.read_bytes,
                        io_write_bytes=io.write_bytes, io_read_time=io.read_time,
                        io_write_time=io.write_time
                    )
                )

            except ValueError:
                self.warning("Exception when formatting: %s" %
                             traceback.format_exc())

        cpu_percent, cpu_times, io, (swap_in, swap_out) = resources(None)
        duration = rm.end_time - rm.start_time

        log_usage('Total resource usage', duration, cpu_percent, cpu_times, io)

        # Print special messages so usage shows up in Treeherder.
        if cpu_percent:
            self._tinderbox_print('CPU usage<br/>{:,.1f}%'.format(
                                  cpu_percent))

        self._tinderbox_print('I/O read bytes / time<br/>{:,} / {:,}'.format(
                              io.read_bytes, io.read_time))
        self._tinderbox_print('I/O write bytes / time<br/>{:,} / {:,}'.format(
                              io.write_bytes, io.write_time))

        # Print CPU components having >1%. "cpu_times" is a data structure
        # whose attributes are measurements. Ideally we'd have an API that
        # returned just the measurements as a dict or something.
        cpu_attrs = []
        for attr in sorted(dir(cpu_times)):
            if attr.startswith('_'):
                continue
            if attr in ('count', 'index'):
                continue
            cpu_attrs.append(attr)

        cpu_total = sum(getattr(cpu_times, attr) for attr in cpu_attrs)

        for attr in cpu_attrs:
            value = getattr(cpu_times, attr)
            percent = value / cpu_total * 100.0
            if percent > 1.00:
                self._tinderbox_print('CPU {}<br/>{:,.1f} ({:,.1f}%)'.format(
                                      attr, value, percent))

        # Swap on Windows isn't reported by psutil.
        if not self._is_windows():
            self._tinderbox_print('Swap in / out<br/>{:,} / {:,}'.format(
                                  swap_in, swap_out))

        for phase in rm.phases.keys():
            start_time, end_time = rm.phases[phase]
            cpu_percent, cpu_times, io, swap = resources(phase)
            log_usage(phase, end_time - start_time, cpu_percent, cpu_times, io)

    def _tinderbox_print(self, message):
        self.info('TinderboxPrint: %s' % message)


class InfluxRecordingMixin(object):
    """Provides InfluxDB stat recording to scripts.

    This class records stats to an InfluxDB server, if enabled. Stat recording
    is enabled in a script by inheriting from this class, and adding an
    influxdb_credentials line to the influx_credentials_file (usually oauth.txt
    in automation).  This line should look something like:

        influxdb_credentials = 'http://goldiewilson-onepointtwentyone-1.c.influxdb.com:8086/db/DBNAME/series?u=DBUSERNAME&p=DBPASSWORD'

    Where DBNAME, DBUSERNAME, and DBPASSWORD correspond to the database name,
    and user/pw credentials for recording to the database. The stats from
    mozharness are recorded in the 'mozharness' table.
    """

    @PreScriptRun
    def influxdb_recording_init(self):
        self.recording = False
        self.post = None
        self.posturl = None
        self.build_metrics_summary = None
        self.res_props = self.config.get('build_resources_path') % self.query_abs_dirs()
        self.info("build_resources.json path: %s" % self.res_props)
        if self.res_props:
            self.rmtree(self.res_props)

        try:
            site_packages_path = self.query_python_site_packages_path()
            if site_packages_path not in sys.path:
                sys.path.append(site_packages_path)

            self.post = get_tlsv1_post()

            auth = os.path.join(os.getcwd(), self.config['influx_credentials_file'])
            if not os.path.exists(auth):
                self.warning("Unable to start influxdb recording: %s not found" % (auth,))
                return
            credentials = {}
            execfile(auth, credentials)
            if 'influxdb_credentials' in credentials:
                self.posturl = credentials['influxdb_credentials']
                self.recording = True
            else:
                self.warning("Unable to start influxdb recording: no credentials")
                return

        except Exception:
            # The exact reason for failing to start stats doesn't really matter.
            # If anything fails, we just won't record stats for this job.
            self.warning("Unable to start influxdb recording: %s" %
                         traceback.format_exc())
            return

    @PreScriptAction
    def influxdb_recording_pre_action(self, action):
        if not self.recording:
            return

        self.start_time = time.time()

    @PostScriptAction
    def influxdb_recording_post_action(self, action, success=None):
        if not self.recording:
            return

        elapsed_time = time.time() - self.start_time

        c = {}
        p = {}
        if self.buildbot_config:
            c = self.buildbot_config.get('properties', {})
        if self.buildbot_properties:
            p = self.buildbot_properties
        self.record_influx_stat([{
            "points": [[
                action,
                elapsed_time,
                c.get('buildername'),
                c.get('product'),
                c.get('platform'),
                c.get('branch'),
                c.get('slavename'),
                c.get('revision'),
                p.get('gaia_revision'),
                c.get('buildid'),
            ]],
            "name": "mozharness",
            "columns": [
                "action",
                "runtime",
                "buildername",
                "product",
                "platform",
                "branch",
                "slavename",
                "gecko_revision",
                "gaia_revision",
                "buildid",
            ],
        }])

    def _get_resource_usage(self, res, name, iolen, cpulen):
        c = {}
        p = {}
        if self.buildbot_config:
            c = self.buildbot_config.get('properties', {})
        if self.buildbot_properties:
            p = self.buildbot_properties

        data = [
            # Build properties
            c.get('buildername'),
            c.get('product'),
            c.get('platform'),
            c.get('branch'),
            c.get('slavename'),
            c.get('revision'),
            p.get('gaia_revision'),
            c.get('buildid'),

            # Mach step properties
            name,
            res.get('start'),
            res.get('end'),
            res.get('duration'),
            res.get('cpu_percent'),
        ]
        # The io and cpu_times fields are arrays, though they aren't always
        # present if a step completes before resource utilization is measured.
        # We add the arrays if they exist, otherwise we just do an array of None
        # to fill up the stat point.
        data.extend(res.get('io', [None] * iolen))
        data.extend(res.get('cpu_times', [None] * cpulen))
        return data

    @PostScriptAction('build')
    def record_mach_stats(self, action, success=None):
        if not os.path.exists(self.res_props):
            self.info('No build_resources.json found, not logging stats')
            return
        with open(self.res_props) as fh:
            resources = json.load(fh)
            data = {
                "points": [
                ],
                "name": "mach",
                "columns": [
                    # Build properties
                    "buildername",
                    "product",
                    "platform",
                    "branch",
                    "slavename",
                    "gecko_revision",
                    "gaia_revision",
                    "buildid",

                    # Mach step properties
                    "name",
                    "start",
                    "end",
                    "duration",
                    "cpu_percent",
                ],
            }

            # The io and cpu_times fields aren't static - they may vary based
            # on the specific platform being measured. Mach records the field
            # names, which we use as the column names here.
            data['columns'].extend(resources['io_fields'])
            data['columns'].extend(resources['cpu_times_fields'])
            iolen = len(resources['io_fields'])
            cpulen = len(resources['cpu_times_fields'])

            if 'duration' in resources:
                self.build_metrics_summary = {
                    'name': 'build times',
                    'value': resources['duration'],
                    'subtests': [],
                }

            # The top-level data has the overall resource usage, which we record
            # under the name 'TOTAL' to separate it from the individual tiers.
            data['points'].append(self._get_resource_usage(resources, 'TOTAL', iolen, cpulen))

            # Each tier also has the same resource stats as the top-level.
            for tier in resources['tiers']:
                data['points'].append(self._get_resource_usage(tier, tier['name'], iolen, cpulen))
                if 'duration' not in tier:
                    self.build_metrics_summary = None
                elif self.build_metrics_summary:
                    self.build_metrics_summary['subtests'].append({
                        'name': tier['name'],
                        'value': tier['duration'],
                    })

            self.record_influx_stat([data])

    def record_influx_stat(self, json_data):
        if not self.recording:
            return
        try:
            r = self.post(self.posturl, data=json.dumps(json_data), timeout=5)
            if r.status_code != 200:
                self.warning("Failed to log stats. Return code = %i, stats = %s" % (r.status_code, json_data))

                # Disable recording for the rest of this job. Even if it's just
                # intermittent, we don't want to keep the build from progressing.
                self.recording = False
        except Exception, e:
            self.warning('Failed to log stats. Exception = %s' % str(e))
            self.recording = False


# __main__ {{{1

if __name__ == '__main__':
    '''TODO: unit tests.
    '''
    pass
