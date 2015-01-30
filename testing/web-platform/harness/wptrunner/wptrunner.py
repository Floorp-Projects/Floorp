# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import json
import logging
import os
import shutil
import socket
import sys
import threading
import time
import urlparse
from Queue import Empty
from StringIO import StringIO

from multiprocessing import Queue

from mozlog.structured import (commandline, stdadapter, get_default_logger,
                               structuredlog, handlers, formatters)

import products
import testloader
import wptcommandline
import wpttest
from testrunner import ManagerGroup

here = os.path.split(__file__)[0]


"""Runner for web-platform-tests

The runner has several design goals:

* Tests should run with no modification from upstream.

* Tests should be regarded as "untrusted" so that errors, timeouts and even
  crashes in the tests can be handled without failing the entire test run.

* For performance tests can be run in multiple browsers in parallel.

The upstream repository has the facility for creating a test manifest in JSON
format. This manifest is used directly to determine which tests exist. Local
metadata files are used to store the expected test results.
"""

logger = None


def setup_logging(args, defaults):
    global logger
    logger = commandline.setup_logging("web-platform-tests", args, defaults)
    setup_stdlib_logger()

    for name in args.keys():
        if name.startswith("log_"):
            args.pop(name)

    return logger


def setup_stdlib_logger():
    logging.root.handlers = []
    logging.root = stdadapter.std_logging_adapter(logging.root)


def do_delayed_imports(serve_root):
    global serve, manifest, sslutils

    sys.path.insert(0, serve_root)
    sys.path.insert(0, str(os.path.join(serve_root, "tools")))
    sys.path.insert(0, str(os.path.join(serve_root, "tools", "scripts")))
    failed = []

    try:
        import serve
    except ImportError:
        failed.append("serve")
    try:
        import manifest
    except ImportError:
        failed.append("manifest")
    try:
        import sslutils
    except ImportError:
        raise
        failed.append("sslutils")

    if failed:
        logger.critical(
            "Failed to import %s. Ensure that tests path %s contains web-platform-tests" %
            (", ".join(failed), serve_root))
        sys.exit(1)


class TestEnvironmentError(Exception):
    pass


class LogLevelRewriter(object):
    """Filter that replaces log messages at specified levels with messages
    at a different level.

    This can be used to e.g. downgrade log messages from ERROR to WARNING
    in some component where ERRORs are not critical.

    :param inner: Handler to use for messages that pass this filter
    :param from_levels: List of levels which should be affected
    :param to_level: Log level to set for the affected messages
    """
    def __init__(self, inner, from_levels, to_level):
        self.inner = inner
        self.from_levels = [item.upper() for item in from_levels]
        self.to_level = to_level.upper()

    def __call__(self, data):
        if data["action"] == "log" and data["level"].upper() in self.from_levels:
            data = data.copy()
            data["level"] = self.to_level
        return self.inner(data)


class TestEnvironment(object):
    def __init__(self, serve_path, test_paths, ssl_env, options):
        """Context manager that owns the test environment i.e. the http and
        websockets servers"""
        self.serve_path = serve_path
        self.test_paths = test_paths
        self.ssl_env = ssl_env
        self.server = None
        self.config = None
        self.external_config = None
        self.test_server_port = options.pop("test_server_port", True)
        self.options = options if options is not None else {}
        self.required_files = options.pop("required_files", [])
        self.files_to_restore = []

    def __enter__(self):
        self.ssl_env.__enter__()
        self.copy_required_files()
        self.setup_server_logging()
        self.setup_routes()
        self.config = self.load_config()
        serve.set_computed_defaults(self.config)
        self.external_config, self.servers = serve.start(self.config, self.ssl_env)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.ssl_env.__exit__(exc_type, exc_val, exc_tb)

        self.restore_files()
        for scheme, servers in self.servers.iteritems():
            for port, server in servers:
                server.kill()

    def load_config(self):
        default_config_path = os.path.join(self.serve_path, "config.default.json")
        local_config_path = os.path.join(here, "config.json")

        with open(default_config_path) as f:
            default_config = json.load(f)

        with open(local_config_path) as f:
            data = f.read()
            local_config = json.loads(data % self.options)

        #TODO: allow non-default configuration for ssl

        local_config["external_host"] = self.options.get("external_host", None)
        local_config["ssl"]["encrypt_after_connect"] = self.options.get("encrypt_after_connect", False)

        config = serve.merge_json(default_config, local_config)
        config["doc_root"] = self.serve_path

        if not self.ssl_env.ssl_enabled:
            config["ports"]["https"] = [None]

        host = self.options.get("certificate_domain", config["host"])
        hosts = [host]
        hosts.extend("%s.%s" % (item[0], host) for item in serve.get_subdomains(host).values())
        key_file, certificate = self.ssl_env.host_cert_path(hosts)

        config["key_file"] = key_file
        config["certificate"] = certificate

        return config

    def setup_server_logging(self):
        server_logger = get_default_logger(component="wptserve")
        assert server_logger is not None
        log_filter = handlers.LogLevelFilter(lambda x:x, "info")
        # Downgrade errors to warnings for the server
        log_filter = LogLevelRewriter(log_filter, ["error"], "warning")
        server_logger.component_filter = log_filter

        serve.logger = server_logger
        #Set as the default logger for wptserve
        serve.set_logger(server_logger)

    def setup_routes(self):
        for url, paths in self.test_paths.iteritems():
            if url == "/":
                continue

            path = paths["tests_path"]
            url = "/%s/" % url.strip("/")

            for (method,
                 suffix,
                 handler_cls) in [(serve.any_method,
                                   b"*.py",
                                   serve.handlers.PythonScriptHandler),
                                  (b"GET",
                                   "*.asis",
                                   serve.handlers.AsIsHandler),
                                  (b"GET",
                                   "*",
                                   serve.handlers.FileHandler)]:
                route = (method, b"%s%s" % (str(url), str(suffix)), handler_cls(path, url_base=url))
                serve.routes.insert(-3, route)

        if "/" not in self.test_paths:
            serve.routes = serve.routes[:-3]

    def copy_required_files(self):
        logger.info("Placing required files in server environment.")
        for source, destination, copy_if_exists in self.required_files:
            source_path = os.path.join(here, source)
            dest_path = os.path.join(self.serve_path, destination, os.path.split(source)[1])
            dest_exists = os.path.exists(dest_path)
            if not dest_exists or copy_if_exists:
                if dest_exists:
                    backup_path = dest_path + ".orig"
                    logger.info("Backing up %s to %s" % (dest_path, backup_path))
                    self.files_to_restore.append(dest_path)
                    shutil.copy2(dest_path, backup_path)
                logger.info("Copying %s to %s" % (source_path, dest_path))
                shutil.copy2(source_path, dest_path)

    def ensure_started(self):
        # Pause for a while to ensure that the server has a chance to start
        time.sleep(2)
        for scheme, servers in self.servers.iteritems():
            for port, server in servers:
                if self.test_server_port:
                    s = socket.socket()
                    try:
                        s.connect((self.config["host"], port))
                    except socket.error:
                        raise EnvironmentError(
                            "%s server on port %d failed to start" % (scheme, port))
                    finally:
                        s.close()

                if not server.is_alive():
                    raise EnvironmentError("%s server on port %d failed to start" % (scheme, port))

    def restore_files(self):
        for path in self.files_to_restore:
            os.unlink(path)
            if os.path.exists(path + ".orig"):
                os.rename(path + ".orig", path)


class LogThread(threading.Thread):
    def __init__(self, queue, logger, level):
        self.queue = queue
        self.log_func = getattr(logger, level)
        threading.Thread.__init__(self, name="Thread-Log")
        self.daemon = True

    def run(self):
        while True:
            try:
                msg = self.queue.get()
            except (EOFError, IOError):
                break
            if msg is None:
                break
            else:
                self.log_func(msg)


class LoggingWrapper(StringIO):
    """Wrapper for file like objects to redirect output to logger
    instead"""

    def __init__(self, queue, prefix=None):
        self.queue = queue
        self.prefix = prefix

    def write(self, data):
        if isinstance(data, str):
            data = data.decode("utf8")

        if data.endswith("\n"):
            data = data[:-1]
        if data.endswith("\r"):
            data = data[:-1]
        if not data:
            return
        if self.prefix is not None:
            data = "%s: %s" % (self.prefix, data)
        self.queue.put(data)

    def flush(self):
        pass

def list_test_groups(serve_root, test_paths, test_types, product, **kwargs):

    do_delayed_imports(serve_root)

    run_info = wpttest.get_run_info(kwargs["run_info"], product, debug=False)
    test_filter = testloader.TestFilter(include=kwargs["include"],
                                        exclude=kwargs["exclude"],
                                        manifest_path=kwargs["include_manifest"])
    test_loader = testloader.TestLoader(test_paths,
                                        test_types,
                                        test_filter,
                                        run_info)

    for item in sorted(test_loader.groups(test_types)):
        print item


def list_disabled(serve_root, test_paths, test_types, product, **kwargs):
    do_delayed_imports(serve_root)
    rv = []
    run_info = wpttest.get_run_info(kwargs["run_info"], product, debug=False)
    test_loader = testloader.TestLoader(test_paths,
                                        test_types,
                                        testloader.TestFilter(),
                                        run_info)

    for test_type, tests in test_loader.disabled_tests.iteritems():
        for test in tests:
            rv.append({"test": test.id, "reason": test.disabled()})
    print json.dumps(rv, indent=2)


def get_ssl_kwargs(**kwargs):
    if kwargs["ssl_type"] == "openssl":
        args = {"openssl_binary": kwargs["openssl_binary"]}
    elif kwargs["ssl_type"] == "pregenerated":
        args = {"host_key_path": kwargs["host_key_path"],
                "host_cert_path": kwargs["host_cert_path"],
                 "ca_cert_path": kwargs["ca_cert_path"]}
    else:
        args = {}
    return args


def run_tests(config, serve_root, test_paths, product, **kwargs):
    logging_queue = None
    logging_thread = None
    original_stdio = (sys.stdout, sys.stderr)
    test_queues = None

    try:
        if not kwargs["no_capture_stdio"]:
            logging_queue = Queue()
            logging_thread = LogThread(logging_queue, logger, "info")
            sys.stdout = LoggingWrapper(logging_queue, prefix="STDOUT")
            sys.stderr = LoggingWrapper(logging_queue, prefix="STDERR")
            logging_thread.start()

        do_delayed_imports(serve_root)

        run_info = wpttest.get_run_info(kwargs["run_info"], product, debug=False)

        (check_args,
         browser_cls, get_browser_kwargs,
         executor_classes, get_executor_kwargs,
         env_options) = products.load_product(config, product)

        check_args(**kwargs)

        ssl_env_cls = sslutils.environments[kwargs["ssl_type"]]
        ssl_env = ssl_env_cls(logger, **get_ssl_kwargs(**kwargs))

        unexpected_total = 0

        if "test_loader" in kwargs:
            test_loader = kwargs["test_loader"]
        else:
            test_filter = testloader.TestFilter(include=kwargs["include"],
                                                exclude=kwargs["exclude"],
                                                manifest_path=kwargs["include_manifest"])
            test_loader = testloader.TestLoader(test_paths,
                                                kwargs["test_types"],
                                                test_filter,
                                                run_info,
                                                kwargs["chunk_type"],
                                                kwargs["total_chunks"],
                                                kwargs["this_chunk"],
                                                kwargs["manifest_update"])

        if kwargs["run_by_dir"] is False:
            test_source_cls = testloader.SingleTestSource
            test_source_kwargs = {}
        else:
            # A value of None indicates infinite depth
            test_source_cls = testloader.PathGroupedSource
            test_source_kwargs = {"depth": kwargs["run_by_dir"]}

        logger.info("Using %i client processes" % kwargs["processes"])

        with TestEnvironment(serve_root,
                             test_paths,
                             ssl_env,
                             env_options) as test_environment:
            try:
                test_environment.ensure_started()
            except TestEnvironmentError as e:
                logger.critical("Error starting test environment: %s" % e.message)
                raise

            browser_kwargs = get_browser_kwargs(ssl_env=ssl_env, **kwargs)
            base_server = "http://%s:%i" % (test_environment.external_config["host"],
                                            test_environment.external_config["ports"]["http"][0])

            repeat = kwargs["repeat"]
            for repeat_count in xrange(repeat):
                if repeat > 1:
                    logger.info("Repetition %i / %i" % (repeat_count + 1, repeat))


                unexpected_count = 0
                logger.suite_start(test_loader.test_ids, run_info)
                for test_type in kwargs["test_types"]:
                    logger.info("Running %s tests" % test_type)

                    for test in test_loader.disabled_tests[test_type]:
                        logger.test_start(test.id)
                        logger.test_end(test.id, status="SKIP")

                    executor_cls = executor_classes.get(test_type)
                    executor_kwargs = get_executor_kwargs(base_server,
                                                          **kwargs)

                    if executor_cls is None:
                        logger.error("Unsupported test type %s for product %s" %
                                     (test_type, product))
                        continue


                    with ManagerGroup("web-platform-tests",
                                      kwargs["processes"],
                                      test_source_cls,
                                      test_source_kwargs,
                                      browser_cls,
                                      browser_kwargs,
                                      executor_cls,
                                      executor_kwargs,
                                      kwargs["pause_on_unexpected"],
                                      kwargs["debug_args"]) as manager_group:
                        try:
                            manager_group.run(test_type, test_loader.tests)
                        except KeyboardInterrupt:
                            logger.critical("Main thread got signal")
                            manager_group.stop()
                            raise
                    unexpected_count += manager_group.unexpected_count()

                unexpected_total += unexpected_count
                logger.info("Got %i unexpected results" % unexpected_count)
                logger.suite_end()
    except KeyboardInterrupt:
        if test_queues is not None:
            for queue in test_queues.itervalues():
                queue.cancel_join_thread()
    finally:
        if test_queues is not None:
            for queue in test_queues.itervalues():
                queue.close()
        sys.stdout, sys.stderr = original_stdio
        if not kwargs["no_capture_stdio"] and logging_queue is not None:
            logger.info("Closing logging queue")
            logging_queue.put(None)
            if logging_thread is not None:
                logging_thread.join(10)
            logging_queue.close()

    return unexpected_total == 0


def main():
    """Main entry point when calling from the command line"""
    kwargs = wptcommandline.parse_args()

    if kwargs["prefs_root"] is None:
        kwargs["prefs_root"] = os.path.abspath(os.path.join(here, "prefs"))

    setup_logging(kwargs, {"raw": sys.stdout})

    if kwargs["list_test_groups"]:
        list_test_groups(**kwargs)
    elif kwargs["list_disabled"]:
        list_disabled(**kwargs)
    else:
        return run_tests(**kwargs)
