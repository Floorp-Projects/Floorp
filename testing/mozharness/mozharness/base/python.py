#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Python usage, esp. virtualenv.
"""

from __future__ import absolute_import, division
import errno
import json
import os
import socket
import sys
import traceback
import subprocess

try:
    import urlparse
except ImportError:
    import urllib.parse as urlparse

from six import string_types

import mozharness
from mozharness.base.errors import VirtualenvErrorList
from mozharness.base.log import FATAL, WARNING
from mozharness.base.script import (
    PostScriptAction,
    PostScriptRun,
    PreScriptAction,
    ScriptMixin,
)

external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    "external_tools",
)


def get_tlsv1_post():
    # Monkeypatch to work around SSL errors in non-bleeding-edge Python.
    # Taken from https://lukasa.co.uk/2013/01/Choosing_SSL_Version_In_Requests/
    import requests
    from requests.packages.urllib3.poolmanager import PoolManager
    import ssl

    class TLSV1Adapter(requests.adapters.HTTPAdapter):
        def init_poolmanager(self, connections, maxsize, block=False):
            self.poolmanager = PoolManager(
                num_pools=connections,
                maxsize=maxsize,
                block=block,
                ssl_version=ssl.PROTOCOL_TLSv1,
            )

    s = requests.Session()
    s.mount("https://", TLSV1Adapter())
    return s.post


# Virtualenv {{{1
virtualenv_config_options = [
    [
        ["--virtualenv-path"],
        {
            "action": "store",
            "dest": "virtualenv_path",
            "default": "venv",
            "help": "Specify the path to the virtualenv top level directory",
        },
    ],
    [
        ["--find-links"],
        {
            "action": "extend",
            "dest": "find_links",
            "default": ["https://pypi.pub.build.mozilla.org/pub/"],
            "help": "URL to look for packages at",
        },
    ],
    [
        ["--pip-index"],
        {
            "action": "store_true",
            "default": False,
            "dest": "pip_index",
            "help": "Use pip indexes",
        },
    ],
    [
        ["--no-pip-index"],
        {
            "action": "store_false",
            "dest": "pip_index",
            "help": "Don't use pip indexes (default)",
        },
    ],
]


class VirtualenvMixin(object):
    """BaseScript mixin, designed to create and use virtualenvs.

    Config items:
     * virtualenv_path points to the virtualenv location on disk.
     * virtualenv_modules lists the module names.
     * MODULE_url list points to the module URLs (optional)
    Requires virtualenv to be in PATH.
    Depends on ScriptMixin
    """

    python_paths = {}
    site_packages_path = None

    def __init__(self, *args, **kwargs):
        self._virtualenv_modules = []
        super(VirtualenvMixin, self).__init__(*args, **kwargs)

    def register_virtualenv_module(
        self,
        name=None,
        url=None,
        method=None,
        requirements=None,
        optional=False,
        two_pass=False,
        editable=False,
    ):
        """Register a module to be installed with the virtualenv.

        This method can be called up until create_virtualenv() to register
        modules that should be installed in the virtualenv.

        See the documentation for install_module for how the arguments are
        applied.
        """
        self._virtualenv_modules.append(
            (name, url, method, requirements, optional, two_pass, editable)
        )

    def query_virtualenv_path(self):
        """Determine the absolute path to the virtualenv."""
        dirs = self.query_abs_dirs()

        if "abs_virtualenv_dir" in dirs:
            return dirs["abs_virtualenv_dir"]

        p = self.config["virtualenv_path"]
        if not p:
            self.fatal(
                "virtualenv_path config option not set; " "this should never happen"
            )

        if os.path.isabs(p):
            return p
        else:
            return os.path.join(dirs["abs_work_dir"], p)

    def query_python_path(self, binary="python"):
        """Return the path of a binary inside the virtualenv, if
        c['virtualenv_path'] is set; otherwise return the binary name.
        Otherwise return None
        """
        if binary not in self.python_paths:
            bin_dir = "bin"
            if self._is_windows():
                bin_dir = "Scripts"
            virtualenv_path = self.query_virtualenv_path()
            self.python_paths[binary] = os.path.abspath(
                os.path.join(virtualenv_path, bin_dir, binary)
            )

        return self.python_paths[binary]

    def query_python_site_packages_path(self):
        if self.site_packages_path:
            return self.site_packages_path
        python = self.query_python_path()
        self.site_packages_path = self.get_output_from_command(
            [
                python,
                "-c",
                "from distutils.sysconfig import get_python_lib; "
                + "print(get_python_lib())",
            ]
        )
        return self.site_packages_path

    def package_versions(
        self, pip_freeze_output=None, error_level=WARNING, log_output=False
    ):
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
            pip_freeze_output = self.get_output_from_command(
                [pip, "list", "--format", "freeze"], silent=True, ignore_errors=True
            )
            if not isinstance(pip_freeze_output, string_types):
                self.fatal(
                    "package_versions: Error encountered running `pip freeze`: "
                    + pip_freeze_output
                )

        for line in pip_freeze_output.splitlines():
            # parse the output into package, version
            line = line.strip()
            if not line:
                # whitespace
                continue
            if line.startswith("-"):
                # not a package, probably like '-e http://example.com/path#egg=package-dev'
                continue
            if "==" not in line:
                self.fatal("pip_freeze_packages: Unrecognized output line: %s" % line)
            package, version = line.split("==", 1)
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
        # pylint --py3k W1655
        package_versions = self.package_versions(error_level=error_level)
        return package_name.lower() in [package.lower() for package in package_versions]

    def install_module(
        self,
        module=None,
        module_url=None,
        install_method=None,
        requirements=(),
        optional=False,
        global_options=[],
        no_deps=False,
        editable=False,
    ):
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
        if install_method in (None, "pip"):
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
            command += ["--timeout", str(c.get("pip_timeout", 120))]
            for requirement in requirements:
                command += ["-r", requirement]
            if c.get("find_links") and not c["pip_index"]:
                command += ["--no-index"]
            for opt in global_options:
                command += ["--global-option", opt]
        elif install_method == "easy_install":
            if not module:
                self.fatal(
                    "module parameter required with install_method='easy_install'"
                )
            if requirements:
                # Install pip requirements files separately, since they're
                # not understood by easy_install.
                self.install_module(requirements=requirements, install_method="pip")
            command = [self.query_python_path(), "-m", "easy_install"]
        else:
            self.fatal(
                "install_module() doesn't understand an install_method of %s!"
                % install_method
            )

        for link in c.get("find_links", []):
            parsed = urlparse.urlparse(link)

            try:
                socket.gethostbyname(parsed.hostname)
            except socket.gaierror as e:
                self.info("error resolving %s (ignoring): %s" % (parsed.hostname, e))
                continue

            command.extend(["--find-links", link])

        # module_url can be None if only specifying requirements files
        if module_url:
            if editable:
                if install_method in (None, "pip"):
                    command += ["-e"]
                else:
                    self.fatal(
                        "editable installs not supported for install_method %s"
                        % install_method
                    )
            command += [module_url]

        # If we're only installing a single requirements file, use
        # the file's directory as cwd, so relative paths work correctly.
        cwd = dirs["abs_work_dir"]
        if not module and len(requirements) == 1:
            cwd = os.path.dirname(requirements[0])

        # Allow for errors while building modules, but require a
        # return status of 0.
        self.retry(
            self.run_command,
            # None will cause default value to be used
            attempts=1 if optional else None,
            good_statuses=(0,),
            error_level=WARNING if optional else FATAL,
            error_message=("Could not install python package: failed all attempts."),
            args=[
                command,
            ],
            kwargs={
                "error_list": VirtualenvErrorList,
                "cwd": cwd,
                "env": env,
                # WARNING only since retry will raise final FATAL if all
                # retry attempts are unsuccessful - and we only want
                # an ERROR of FATAL if *no* retry attempt works
                "error_level": WARNING,
            },
        )

    def create_virtualenv(self, modules=(), requirements=()):
        """
        Create a python virtualenv.

        This uses the copy of virtualenv that is vendored in mozharness.

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

        # Always use the virtualenv that is vendored since that is deterministic.
        # TODO Bug 1408051 - Use the copy of virtualenv under
        # third_party/python/virtualenv once everything is off buildbot
        # base_work_dir is for when we're running with mozharness.zip, e.g. on
        # test jobs
        # abs_src_dir is for when we're running out of a checked out copy of
        # the source code
        venv_search_dirs = [
            os.path.join("{base_work_dir}", "mozharness"),
            "{abs_src_dir}",
        ]
        if "abs_src_dir" not in dirs and "repo_path" in self.config:
            dirs["abs_src_dir"] = os.path.normpath(self.config["repo_path"])
        for d in venv_search_dirs:
            file = os.path.join(
                d, "third_party", "python", "virtualenv", "virtualenv.py"
            )
            try:
                venv_py_path = file.format(**dirs)
            except KeyError:
                continue
            if os.path.exists(venv_py_path):
                break
        else:
            self.fatal("Can't find the virtualenv module")

        virtualenv = [
            sys.executable,
            venv_py_path,
        ]
        virtualenv_options = c.get("virtualenv_options", [])
        # Creating symlinks in the virtualenv may cause issues during
        # virtualenv creation or operation on non-Redhat derived
        # distros. On Redhat derived distros --always-copy causes
        # imports to fail. See
        # https://github.com/pypa/virtualenv/issues/565. Therefore
        # only use --alway-copy when not using Redhat.
        if self._is_redhat_based():
            self.warning(
                "creating virtualenv without --always-copy "
                "due to issues on Redhat derived distros"
            )
        else:
            virtualenv_options.append("--always-copy")

        if os.path.exists(self.query_python_path()):
            self.info(
                "Virtualenv %s appears to already exist; "
                "skipping virtualenv creation." % self.query_python_path()
            )
        else:
            self.mkdir_p(dirs["abs_work_dir"])
            self.run_command(
                virtualenv + virtualenv_options + [venv_path],
                cwd=dirs["abs_work_dir"],
                error_list=VirtualenvErrorList,
                partial_env={"VIRTUALENV_NO_DOWNLOAD": "1"},
                halt_on_failure=True,
            )
        self.info(self.platform_name())
        if self.platform_name().startswith("macos"):
            tmp_path = "{}/bin/bak".format(venv_path)
            self.info(
                "Copying venv python binaries to {} to clear for re-sign".format(
                    tmp_path
                )
            )
            subprocess.call("mkdir -p {}".format(tmp_path), shell=True)
            subprocess.call(
                "cp {}/bin/python* {}/".format(venv_path, tmp_path), shell=True
            )
            self.info("Replacing venv python binaries with reset copies")
            subprocess.call(
                "mv -f {}/* {}/bin/".format(tmp_path, venv_path), shell=True
            )
            self.info(
                "codesign -s - --preserve-metadata=identifier,entitlements,flags,runtime "
                "-f {}/bin/*".format(venv_path)
            )
            subprocess.call(
                "codesign -s - --preserve-metadata=identifier,entitlements,flags,runtime -f "
                "{}/bin/python*".format(venv_path),
                shell=True,
            )

        if not modules:
            modules = c.get("virtualenv_modules", [])
        if not requirements:
            requirements = c.get("virtualenv_requirements", [])
        if not modules and requirements:
            self.install_module(requirements=requirements, install_method="pip")
        for module in modules:
            module_url = module
            global_options = []
            if isinstance(module, dict):
                if module.get("name", None):
                    module_name = module["name"]
                else:
                    self.fatal(
                        "Can't install module without module name: %s" % str(module)
                    )
                module_url = module.get("url", None)
                global_options = module.get("global_options", [])
            else:
                module_url = self.config.get("%s_url" % module, module_url)
                module_name = module
            install_method = "pip"
            if module_name in ("pywin32",):
                install_method = "easy_install"
            self.install_module(
                module=module_name,
                module_url=module_url,
                install_method=install_method,
                requirements=requirements,
                global_options=global_options,
            )

        for (
            module,
            url,
            method,
            requirements,
            optional,
            two_pass,
            editable,
        ) in self._virtualenv_modules:
            if two_pass:
                self.install_module(
                    module=module,
                    module_url=url,
                    install_method=method,
                    requirements=requirements or (),
                    optional=optional,
                    no_deps=True,
                    editable=editable,
                )
            self.install_module(
                module=module,
                module_url=url,
                install_method=method,
                requirements=requirements or (),
                optional=optional,
                editable=editable,
            )

        self.info("Done creating virtualenv %s." % venv_path)

        self.package_versions(log_output=True)

    def activate_virtualenv(self):
        """Import the virtualenv's packages into this Python interpreter."""
        bin_dir = os.path.dirname(self.query_python_path())
        activate = os.path.join(bin_dir, "activate_this.py")
        exec(open(activate).read(), dict(__file__=activate))


# This is (sadly) a mixin for logging methods.
class PerfherderResourceOptionsMixin(ScriptMixin):
    def perfherder_resource_options(self):
        """Obtain a list of extraOptions values to identify the env."""
        opts = []

        if "TASKCLUSTER_INSTANCE_TYPE" in os.environ:
            # Include the instance type so results can be grouped.
            opts.append("taskcluster-%s" % os.environ["TASKCLUSTER_INSTANCE_TYPE"])
        else:
            # We assume !taskcluster => buildbot.
            instance = "unknown"

            # Try to load EC2 instance type from metadata file. This file
            # may not exist in many scenarios (including when inside a chroot).
            # So treat it as optional.
            try:
                # This file should exist on Linux in EC2.
                with open("/etc/instance_metadata.json", "rb") as fh:
                    im = json.load(fh)
                    instance = im.get("aws_instance_type", u"unknown").encode("ascii")
            except IOError as e:
                if e.errno != errno.ENOENT:
                    raise
                self.info(
                    "instance_metadata.json not found; unable to "
                    "determine instance type"
                )
            except Exception:
                self.warning(
                    "error reading instance_metadata: %s" % traceback.format_exc()
                )

            opts.append("buildbot-%s" % instance)

        return opts


class ResourceMonitoringMixin(PerfherderResourceOptionsMixin):
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

        self.register_virtualenv_module("psutil>=5.6.3", method="pip", optional=True)
        self.register_virtualenv_module(
            "mozsystemmonitor==0.4", method="pip", optional=True
        )
        self.register_virtualenv_module("jsonschema==2.5.1", method="pip")
        self._resource_monitor = None

        # 2-tuple of (name, options) to assign Perfherder resource monitor
        # metrics to. This needs to be assigned by a script in order for
        # Perfherder metrics to be reported.
        self.resource_monitor_perfherder_id = None

    @PostScriptAction("create-virtualenv")
    def _start_resource_monitoring(self, action, success=None):
        self.activate_virtualenv()

        # Resource Monitor requires Python 2.7, however it's currently optional.
        # Remove when all machines have had their Python version updated (bug 711299).
        if sys.version_info[:2] < (2, 7):
            self.warning(
                "Resource monitoring will not be enabled! Python 2.7+ required."
            )
            return

        try:
            from mozsystemmonitor.resourcemonitor import SystemResourceMonitor

            self.info("Starting resource monitoring.")
            self._resource_monitor = SystemResourceMonitor(poll_interval=1.0)
            self._resource_monitor.start()
        except Exception:
            self.warning(
                "Unable to start resource monitor: %s" % traceback.format_exc()
            )

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
                upload_dir = self.query_abs_dirs()["abs_blob_upload_dir"]
                if not os.path.exists(upload_dir):
                    os.makedirs(upload_dir)
                with open(os.path.join(upload_dir, "resource-usage.json"), "w") as fh:
                    json.dump(
                        self._resource_monitor.as_dict(), fh, sort_keys=True, indent=4
                    )
            except (AttributeError, KeyError):
                self.exception("could not upload resource usage JSON", level=WARNING)

        except Exception:
            self.warning(
                "Exception when reporting resource usage: %s" % traceback.format_exc()
            )

    def _log_resource_usage(self):
        # Delay import because not available until virtualenv is populated.
        import jsonschema

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
            message = (
                "{prefix} - Wall time: {duration:.0f}s; "
                "CPU: {cpu_percent}; "
                "Read bytes: {io_read_bytes}; Write bytes: {io_write_bytes}; "
                "Read time: {io_read_time}; Write time: {io_write_time}"
            )

            # XXX Some test harnesses are complaining about a string being
            # being fed into a 'f' formatter. This will help diagnose the
            # issue.
            if cpu_percent:
                # pylint: disable=W1633
                cpu_percent_str = str(round(cpu_percent)) + "%"
            else:
                cpu_percent_str = "Can't collect data"

            try:
                self.info(
                    message.format(
                        prefix=prefix,
                        duration=duration,
                        cpu_percent=cpu_percent_str,
                        io_read_bytes=io.read_bytes,
                        io_write_bytes=io.write_bytes,
                        io_read_time=io.read_time,
                        io_write_time=io.write_time,
                    )
                )

            except ValueError:
                self.warning("Exception when formatting: %s" % traceback.format_exc())

        cpu_percent, cpu_times, io, (swap_in, swap_out) = resources(None)
        duration = rm.end_time - rm.start_time

        # Write out Perfherder data if configured.
        if self.resource_monitor_perfherder_id:
            perfherder_name, perfherder_options = self.resource_monitor_perfherder_id

            suites = []
            overall = []

            if cpu_percent:
                overall.append(
                    {
                        "name": "cpu_percent",
                        "value": cpu_percent,
                    }
                )

            overall.extend(
                [
                    {"name": "io_write_bytes", "value": io.write_bytes},
                    {"name": "io.read_bytes", "value": io.read_bytes},
                    {"name": "io_write_time", "value": io.write_time},
                    {"name": "io_read_time", "value": io.read_time},
                ]
            )

            suites.append(
                {
                    "name": "%s.overall" % perfherder_name,
                    "extraOptions": perfherder_options
                    + self.perfherder_resource_options(),
                    "subtests": overall,
                }
            )

            for phase in rm.phases.keys():
                phase_duration = rm.phases[phase][1] - rm.phases[phase][0]
                subtests = [
                    {
                        "name": "time",
                        "value": phase_duration,
                    }
                ]
                cpu_percent = rm.aggregate_cpu_percent(phase=phase, per_cpu=False)
                if cpu_percent is not None:
                    subtests.append(
                        {
                            "name": "cpu_percent",
                            "value": rm.aggregate_cpu_percent(
                                phase=phase, per_cpu=False
                            ),
                        }
                    )

                # We don't report I/O during each step because measured I/O
                # is system I/O and that I/O can be delayed (e.g. writes will
                # buffer before being flushed and recorded in our metrics).
                suites.append(
                    {
                        "name": "%s.%s" % (perfherder_name, phase),
                        "subtests": subtests,
                    }
                )

            data = {
                "framework": {"name": "job_resource_usage"},
                "suites": suites,
            }

            schema_path = os.path.join(
                external_tools_path, "performance-artifact-schema.json"
            )
            with open(schema_path, "rb") as fh:
                schema = json.load(fh)

            # this will throw an exception that causes the job to fail if the
            # perfherder data is not valid -- please don't change this
            # behaviour, otherwise people will inadvertently break this
            # functionality
            self.info("Validating Perfherder data against %s" % schema_path)
            jsonschema.validate(data, schema)
            self.info("PERFHERDER_DATA: %s" % json.dumps(data))

        log_usage("Total resource usage", duration, cpu_percent, cpu_times, io)

        # Print special messages so usage shows up in Treeherder.
        if cpu_percent:
            self._tinderbox_print("CPU usage<br/>{:,.1f}%".format(cpu_percent))

        self._tinderbox_print(
            "I/O read bytes / time<br/>{:,} / {:,}".format(io.read_bytes, io.read_time)
        )
        self._tinderbox_print(
            "I/O write bytes / time<br/>{:,} / {:,}".format(
                io.write_bytes, io.write_time
            )
        )

        # Print CPU components having >1%. "cpu_times" is a data structure
        # whose attributes are measurements. Ideally we'd have an API that
        # returned just the measurements as a dict or something.
        cpu_attrs = []
        for attr in sorted(dir(cpu_times)):
            if attr.startswith("_"):
                continue
            if attr in ("count", "index"):
                continue
            cpu_attrs.append(attr)

        cpu_total = sum(getattr(cpu_times, attr) for attr in cpu_attrs)

        for attr in cpu_attrs:
            value = getattr(cpu_times, attr)
            # cpu_total can be 0.0. Guard against division by 0.
            # pylint --py3k W1619
            percent = value / cpu_total * 100.0 if cpu_total else 0.0

            if percent > 1.00:
                self._tinderbox_print(
                    "CPU {}<br/>{:,.1f} ({:,.1f}%)".format(attr, value, percent)
                )

        # Swap on Windows isn't reported by psutil.
        if not self._is_windows():
            self._tinderbox_print(
                "Swap in / out<br/>{:,} / {:,}".format(swap_in, swap_out)
            )

        for phase in rm.phases.keys():
            start_time, end_time = rm.phases[phase]
            cpu_percent, cpu_times, io, swap = resources(phase)
            log_usage(phase, end_time - start_time, cpu_percent, cpu_times, io)

    def _tinderbox_print(self, message):
        self.info("TinderboxPrint: %s" % message)


# This needs to be inherited only if you have already inherited ScriptMixin
class Python3Virtualenv(object):
    """Support Python3.5+ virtualenv creation."""

    py3_initialized_venv = False

    def py3_venv_configuration(self, python_path, venv_path):
        """We don't use __init__ to allow integrating with other mixins.

        python_path - Path to Python 3 binary.
        venv_path - Path to virtual environment to be created.
        """
        self.py3_initialized_venv = True
        self.py3_python_path = os.path.abspath(python_path)
        version = self.get_output_from_command(
            [self.py3_python_path, "--version"], env=self.query_env()
        ).split()[-1]
        # Using -m venv is only used on 3.5+ versions
        assert version > "3.5.0"
        self.py3_venv_path = os.path.abspath(venv_path)
        self.py3_pip_path = os.path.join(self.py3_path_to_executables(), "pip")

    def py3_path_to_executables(self):
        platform = self.platform_name()
        if platform.startswith("win"):
            return os.path.join(self.py3_venv_path, "Scripts")
        else:
            return os.path.join(self.py3_venv_path, "bin")

    def py3_venv_initialized(func):
        def call(self, *args, **kwargs):
            if not self.py3_initialized_venv:
                raise Exception(
                    "You need to call py3_venv_configuration() "
                    "before using this method."
                )
            func(self, *args, **kwargs)

        return call

    @py3_venv_initialized
    def py3_create_venv(self):
        """Create Python environment with python3 -m venv /path/to/venv."""
        if os.path.exists(self.py3_venv_path):
            self.info(
                "Virtualenv %s appears to already exist; skipping "
                "virtualenv creation." % self.py3_venv_path
            )
        else:
            self.info("Running command...")
            self.run_command(
                "%s -m venv %s" % (self.py3_python_path, self.py3_venv_path),
                error_list=VirtualenvErrorList,
                halt_on_failure=True,
                env=self.query_env(),
            )

    @py3_venv_initialized
    def py3_install_modules(self, modules, use_mozharness_pip_config=True):
        if not os.path.exists(self.py3_venv_path):
            raise Exception("You need to call py3_create_venv() first.")

        for m in modules:
            cmd = [self.py3_pip_path, "install"]
            if use_mozharness_pip_config:
                cmd += self._mozharness_pip_args()
            cmd += [m]
            self.run_command(cmd, env=self.query_env())

    def _mozharness_pip_args(self):
        """We have information in Mozharness configs that apply to pip"""
        c = self.config
        pip_args = []
        # To avoid timeouts with our pypi server, increase default timeout:
        # https://bugzilla.mozilla.org/show_bug.cgi?id=1007230#c802
        pip_args += ["--timeout", str(c.get("pip_timeout", 120))]

        if c.get("find_links") and not c["pip_index"]:
            pip_args += ["--no-index"]

        # Add --find-links pages to look at. Add --trusted-host automatically if
        # the host isn't secure. This allows modern versions of pip to connect
        # without requiring an override.
        trusted_hosts = set()
        for link in c.get("find_links", []):
            parsed = urlparse.urlparse(link)

            try:
                socket.gethostbyname(parsed.hostname)
            except socket.gaierror as e:
                self.info("error resolving %s (ignoring): %s" % (parsed.hostname, e))
                continue

            pip_args += ["--find-links", link]
            if parsed.scheme != "https":
                trusted_hosts.add(parsed.hostname)

        for host in sorted(trusted_hosts):
            pip_args += ["--trusted-host", host]

        return pip_args

    @py3_venv_initialized
    def py3_install_requirement_files(
        self, requirements, pip_args=[], use_mozharness_pip_config=True
    ):
        """
        requirements - You can specify multiple requirements paths
        """
        cmd = [self.py3_pip_path, "install"]
        cmd += pip_args

        if use_mozharness_pip_config:
            cmd += self._mozharness_pip_args()

        for requirement_path in requirements:
            cmd += ["-r", requirement_path]

        self.run_command(cmd, env=self.query_env())


# __main__ {{{1

if __name__ == "__main__":
    """TODO: unit tests."""
    pass
