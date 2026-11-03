#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Runner provenance and real loopback fixture tests; no native qualification."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
from http.server import ThreadingHTTPServer
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen


spec = importlib.util.spec_from_file_location(
    "browser_test_runner", Path(__file__).with_name("run-browser-tests.py"))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class BrowserProvenanceTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="floorp-runner-provenance-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        replacement = patch.object(runner, "ROOT", self.root)
        replacement.start()
        self.addCleanup(replacement.stop)
        self.entry = self.root / "fixtures/entry.mts"
        self.helper = self.root / "fixtures/helper.mts"
        self.entry.parent.mkdir()
        self.entry.write_bytes(b'import { value } from "./helper.mts";\nexport { value };\n')
        self.helper.write_bytes(b'export const value = "before";\n')

    def metafile(self):
        path = self.root / "bundle.meta.json"
        path.write_text(json.dumps({"inputs": {
            str(path.relative_to(self.root)): {"bytes": path.stat().st_size}
            for path in (self.helper, self.entry)
        }}))
        return path

    def actors(self):
        source = self.root / "source/FixtureChild.sys.mjs"
        source.parent.mkdir(exist_ok=True)
        source.write_bytes(b"export const authoredActorValue = 1;\n")
        loaded = self.root / "profile/chrome/runtime-actors"
        retained = self.root / "proof/actor-modules"
        loaded.mkdir(parents=True, exist_ok=True)
        retained.mkdir(parents=True, exist_ok=True)
        return source, loaded, retained

    def execution(self):
        bundles = self.root / "modules"
        bundles.mkdir()
        metafile = self.metafile().rename(bundles / "test-0.meta.json")
        bundle = bundles / "test-0.mjs"
        bundle.write_bytes(b"// Authored provenance fixture; never executed.\n")
        record = {
            "bundleInputs": runner.bundle_input_records(metafile),
            "bundleMetafileSha256": runner.file_record(metafile)["sha256"],
            "bundleSha256": runner.file_record(bundle)["sha256"],
        }
        runner.verify_execution_sources([record], bundles, [])
        return record, bundles, metafile, bundle

    def test_file_record_binds_canonical_path_actual_hash_and_byte_count(self):
        self.helper.write_bytes("export const value = '日本語';\n".encode("utf-8"))
        alias = self.helper.parent / ".." / "fixtures" / self.helper.name
        record = runner.file_record(alias)
        self.assertEqual(record, {
            "path": str(self.helper.resolve()),
            "sha256": hashlib.sha256(self.helper.read_bytes()).hexdigest(),
            "bytes": len(self.helper.read_bytes()),
        })
        runner.verify_file_records([record])

    def test_bundle_inputs_include_imported_helper_sorted_by_canonical_path(self):
        records = runner.bundle_input_records(self.metafile())
        self.assertEqual(records, [runner.file_record(path)
                                   for path in sorted((self.entry, self.helper))])
        runner.verify_file_records(records)

    def native_esbuild(self):
        repository = Path(__file__).resolve().parents[2]
        entries = sorted((repository / "node_modules/.deno").glob(
            "esbuild@*/node_modules/esbuild/lib/main.js"))
        entries.append(repository / "node_modules/esbuild/lib/main.js")
        package = next((entry for entry in entries if entry.is_file()), None)
        self.assertIsNotNone(package, "Install project dependencies before testing real esbuild")
        resolved = subprocess.check_output([
            "node", "-e",
            "const path = require('node:path'); "
            "const name = '@esbuild/' + process.platform + '-' + process.arch + '/' + "
            "(process.platform === 'win32' ? 'esbuild.exe' : 'bin/esbuild'); "
            "process.stdout.write(require.resolve(name, {paths: [path.dirname(process.argv[1])]}));",
            str(package),
        ], text=True)
        executable = Path(resolved).resolve(strict=True)
        self.assertEqual(runner.esbuild_command(executable), [str(executable)])
        return executable

    def actual_bundle(self, executable):
        globals_file = self.root / "browser-globals.mjs"
        globals_file.write_bytes(b"export const authoredGlobal = globalThis.authoredGlobal;\n")
        bundle = self.root / "bundle.mjs"
        metafile = self.root / "bundle.meta.json"
        record = runner.bundle_test(executable, self.entry, globals_file, bundle, metafile)
        expected = sorted((self.entry, self.helper, globals_file))
        self.assertEqual(record["bundleInputs"], [runner.file_record(path) for path in expected])
        self.assertEqual(record["testSourceSha256"], runner.file_record(self.entry)["sha256"])
        self.assertEqual(record["bundleSha256"], runner.file_record(bundle)["sha256"])
        self.assertEqual(record["bundleMetafileSha256"], runner.file_record(metafile)["sha256"])
        actual = subprocess.check_output([
            "node", "--input-type=module", "-e",
            "import { value } from " + json.dumps(bundle.as_uri()) + "; console.log(value);",
        ], text=True).strip()
        self.assertEqual(actual, "before")
        self.helper.write_bytes(self.helper.read_bytes().replace(b"before", b"after!"))
        with self.assertRaises(RuntimeError):
            runner.verify_file_records(record["bundleInputs"])

    def test_actual_native_esbuild_bundles_imports_and_preserves_complete_provenance(self):
        self.actual_bundle(self.native_esbuild())

    def test_javascript_launcher_runs_actual_native_esbuild_and_preserves_provenance(self):
        executable = self.native_esbuild()
        # Exercise a real Node launcher even when npm replaced the installed
        # wrapper with its native executable. The actual bundler still runs.
        launcher = self.root / "esbuild-node-launcher"
        launcher.write_text(
            "#!/usr/bin/env node\n"
            "require('node:child_process').execFileSync(" + json.dumps(str(executable)) +
            ", process.argv.slice(2), {stdio: 'inherit'});\n", encoding="utf-8")
        self.actual_bundle(launcher)

    def test_native_package_formats_do_not_run_as_javascript(self):
        for magic in (b"\x7fELF", b"MZ\x90\0", b"\xfe\xed\xfa\xce",
                      b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf",
                      b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe",
                      b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf",
                      b"\xbf\xba\xfe\xca"):
            with self.subTest(magic=magic.hex()):
                executable = self.root / "esbuild-package-entry"
                executable.write_bytes(magic)
                self.assertEqual(runner.esbuild_command(executable), [str(executable)])

    def test_changed_import_is_rejected_when_entry_bytes_are_unchanged(self):
        entry_before = self.entry.read_bytes()
        records = runner.bundle_input_records(self.metafile())
        self.helper.write_bytes(self.helper.read_bytes().replace(b"before", b"after!"))
        self.assertEqual(self.entry.read_bytes(), entry_before)
        with self.assertRaises(RuntimeError):
            runner.verify_file_records(records)

    def test_missing_metafile_input_is_rejected(self):
        metafile = self.metafile()
        self.helper.unlink()
        with self.assertRaises((RuntimeError, OSError)):
            runner.bundle_input_records(metafile)

    def test_metafile_byte_count_must_match_actual_input(self):
        metafile = self.metafile()
        metadata = json.loads(metafile.read_text())
        metadata["inputs"][str(self.helper.relative_to(self.root))]["bytes"] += 1
        metafile.write_text(json.dumps(metadata))
        with self.assertRaises(RuntimeError):
            runner.bundle_input_records(metafile)

    def test_input_alias_cannot_duplicate_a_canonical_source(self):
        metafile = self.metafile()
        metadata = json.loads(metafile.read_text())
        metadata["inputs"]["fixtures/../fixtures/helper.mts"] = {
            "bytes": self.helper.stat().st_size,
        }
        metafile.write_text(json.dumps(metadata))
        with self.assertRaises(ValueError):
            runner.bundle_input_records(metafile)

    def test_recorded_file_removal_is_rejected(self):
        records = runner.bundle_input_records(self.metafile())
        self.helper.unlink()
        with self.assertRaises((RuntimeError, OSError)):
            runner.verify_file_records(records)

    def test_recorded_size_is_checked_independently_of_hash(self):
        record = runner.file_record(self.helper)
        record["bytes"] += 1
        with self.assertRaises(RuntimeError):
            runner.verify_file_records([record])

    def test_modified_metafile_bytes_are_rejected_even_with_identical_inputs(self):
        record, bundles, metafile, _bundle = self.execution()
        original = json.loads(metafile.read_text())
        metafile.write_text(metafile.read_text() + "\n")
        self.assertEqual(json.loads(metafile.read_text()), original)
        with self.assertRaises(RuntimeError):
            runner.verify_execution_sources([record], bundles, [])

    def test_modified_output_bundle_is_rejected_with_unchanged_sources(self):
        record, bundles, _metafile, bundle = self.execution()
        bundle.write_bytes(bundle.read_bytes().replace(b"never", b"NEVER"))
        runner.verify_file_records(record["bundleInputs"])
        with self.assertRaises(RuntimeError):
            runner.verify_execution_sources([record], bundles, [])

    def test_unchanged_actor_source_loaded_copy_and_retained_copy_pass(self):
        source, loaded, retained = self.actors()
        record = runner.copy_actor_module(source, loaded, retained)
        self.assertEqual(record["path"], str(source.resolve()))
        self.assertEqual(Path(record["loadedPath"]).parent, loaded.resolve())
        self.assertEqual(Path(record["copiedPath"]).parent, retained.resolve())
        self.assertNotEqual(record["loadedPath"], record["copiedPath"])
        for key in ("path", "loadedPath", "copiedPath"):
            self.assertEqual(Path(record[key]).read_bytes(), source.read_bytes())
        runner.verify_actor_modules([record])

    def test_changed_original_actor_is_rejected_even_with_intact_copies(self):
        source, loaded, retained = self.actors()
        record = runner.copy_actor_module(source, loaded, retained)
        original = source.read_bytes()
        source.write_bytes(original.replace(b"= 1", b"= 2"))
        self.assertEqual(Path(record["loadedPath"]).read_bytes(), original)
        self.assertEqual(Path(record["copiedPath"]).read_bytes(), original)
        with self.assertRaises(RuntimeError):
            runner.verify_actor_modules([record])

    def test_tampered_loaded_or_retained_actor_is_rejected(self):
        source, loaded, retained = self.actors()
        for key in ("loadedPath", "copiedPath"):
            with self.subTest(copy=key):
                record = runner.copy_actor_module(source, loaded, retained)
                target = Path(record[key])
                target.write_bytes(target.read_bytes().replace(b"= 1", b"= 3"))
                with self.assertRaises(RuntimeError):
                    runner.verify_actor_modules([record])

    def test_missing_loaded_or_retained_actor_is_rejected(self):
        source, loaded, retained = self.actors()
        for key in ("loadedPath", "copiedPath"):
            with self.subTest(copy=key):
                record = runner.copy_actor_module(source, loaded, retained)
                Path(record[key]).unlink()
                with self.assertRaises((RuntimeError, OSError)):
                    runner.verify_actor_modules([record])


class ParserBarrierTests(unittest.TestCase):
    def setUp(self):
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), runner.FixtureHandler)
        self.server.user_scripts_barriers = runner.UserScriptsParserBarriers()
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = "http://127.0.0.1:" + str(self.server.server_port)
        self.addCleanup(self.close_server)
        self.token = "0123456789abcdef" * 2

    def close_server(self):
        self.server.user_scripts_barriers.close()
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)
        self.assertFalse(self.thread.is_alive())

    def request(self, operation, token=None, method="GET"):
        url = self.base + "/user-scripts-consent/" + operation
        if token is not None:
            url += "?token=" + token
        with urlopen(Request(url, method=method), timeout=3) as response:
            return response.status, response.headers, response.read()

    def status(self):
        return json.loads(self.request("status", self.token)[2])

    def await_script_request(self):
        deadline = time.monotonic() + 2
        while not self.status()["scriptRequested"]:
            if time.monotonic() >= deadline:
                self.fail("The real external-script HTTP request was not received")
            threading.Event().wait(0.005)

    def test_actual_script_request_blocks_until_http_release(self):
        status, headers, body = self.request("held.html", self.token)
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertIn(b'<script src="/user-scripts-consent/barrier.js?token=', body)
        self.assertLess(body.index(b"<script"), body.index(b"<main"))
        with ThreadPoolExecutor(max_workers=1) as pool:
            pending = pool.submit(self.request, "barrier.js", self.token)
            try:
                self.await_script_request()
                self.assertFalse(pending.done())
                self.assertEqual(self.status(), {
                    "scriptRequested": True, "released": False, "timedOut": False,
                })
            finally:
                self.request("release", self.token, method="POST")
            self.assertIn(b"barrier completed", pending.result(timeout=2)[2])
        self.assertEqual(self.status(), {
            "scriptRequested": True, "released": True, "timedOut": False,
        })

    def test_close_unblocks_owned_pending_request_without_claiming_ui_release(self):
        self.request("held.html", self.token)
        with ThreadPoolExecutor(max_workers=1) as pool:
            pending = pool.submit(self.request, "barrier.js", self.token)
            try:
                self.await_script_request()
                self.assertFalse(pending.done())
            finally:
                self.server.user_scripts_barriers.close()
            self.assertEqual(pending.result(timeout=2)[0], 200)
        self.assertEqual(self.status(), {
            "scriptRequested": True, "released": False, "timedOut": False,
        })

    def test_token_reuse_unknown_tokens_and_wrong_methods_are_rejected(self):
        self.assertEqual(self.status(), {
            "scriptRequested": False, "released": False, "timedOut": False,
        })
        for operation, method, expected in (
            ("release", "POST", 404), ("barrier.js", "GET", 404),
            ("release", "GET", 405), ("held.html", "POST", 405),
        ):
            with self.subTest(operation=operation, method=method):
                with self.assertRaises(HTTPError) as failure:
                    self.request(operation, self.token, method)
                self.assertEqual(failure.exception.code, expected)
                failure.exception.close()
        self.request("held.html", self.token)
        with self.assertRaises(HTTPError) as failure:
            self.request("held.html", self.token)
        self.assertEqual(failure.exception.code, 409)
        failure.exception.close()

    def test_malformed_and_ambiguous_tokens_are_rejected(self):
        for token in (None, "", "A" * 32, "0" * 31, self.token + "&token=" + self.token):
            with self.subTest(token=token):
                with self.assertRaises(HTTPError) as failure:
                    self.request("held.html", token)
                self.assertEqual(failure.exception.code, 400)
                failure.exception.close()

    def test_number_of_parser_states_is_bounded(self):
        for value in range(64):
            self.request("held.html", format(value, "032x"))
        with self.assertRaises(HTTPError) as failure:
            self.request("held.html", "f" * 32)
        self.assertEqual(failure.exception.code, 409)
        failure.exception.close()


if __name__ == "__main__":
    unittest.main()
