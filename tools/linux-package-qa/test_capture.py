# SPDX-License-Identifier: MPL-2.0
import copy
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

import capture
from test_intake import context_fixture


def fixture():
    index, request, original = context_fixture()
    floorp = "/repos/Floorp-Projects/Floorp/actions"
    runtime = "/repos/Floorp-Projects/Floorp-Runtime/actions"
    identities = {"index": request["indexArtifact"], "source": index["source"]["artifact"], "package": index["packageArtifact"], "nativeRecord": index["nativeRecordArtifact"], "provenance": index["runtime"]["provenanceArtifact"], "manifest": index["runtime"]["manifestArtifact"], "runtimeNative": index["runtime"]["nativeArtifact"]}
    artifacts = {name: {**identity, "expired": False, "size_in_bytes": 100, "workflow_run": {"id": 122 if name in {"manifest", "runtimeNative"} else 123, "head_sha": "b" * 40 if name in {"manifest", "runtimeNative"} else "a" * 40}} for name, identity in identities.items()}
    native = [artifacts["runtimeNative"]]
    for n, name in enumerate(capture.TARGET_NAMES):
        if name == native[0]["name"]:
            continue
        native.append({"id": 2000 + n, "name": name, "digest": "sha256:" + hashlib.sha256(name.encode()).hexdigest(), "expired": False, "size_in_bytes": 100, "workflow_run": {"id": 122, "head_sha": "b" * 40}})
    original["runtime"]["created_at"] = "2026-10-14T10:42:53Z"
    p_artifacts = [value for name, value in artifacts.items() if name not in {"manifest", "runtimeNative"}]
    r_artifacts = [artifacts["manifest"], *native]
    data = {
        f"{floorp}/runs/123": original["producer"], f"{floorp}/runs/124": original["consumer"],
        f"{runtime}/runs/122": original["runtime"],
        f"{floorp}/runs/123/jobs?per_page=100&page=1": original["producerJobPages"][0],
        f"{floorp}/runs/123/artifacts?per_page=100&page=1": {"total_count": len(p_artifacts), "artifacts": p_artifacts},
        f"{runtime}/runs/122/artifacts?per_page=100&page=1": {"total_count": len(r_artifacts), "artifacts": r_artifacts},
    }
    env = {"GITHUB_ACTIONS": "true", "GITHUB_REPOSITORY": "Floorp-Projects/Floorp", "GITHUB_REPOSITORY_ID": "437505831", "GITHUB_RUN_ATTEMPT": "1", "GITHUB_RUN_ID": "124", "GITHUB_SHA": "a" * 40}
    return index, request, data, env


class FakeApi:
    def __init__(self, values):
        self.values = values
        self.calls = []

    def get(self, endpoint):
        self.calls.append(endpoint)
        if not capture.ENDPOINT.fullmatch(endpoint):
            raise AssertionError("non-metadata endpoint requested")
        return copy.deepcopy(self.values[endpoint])


class CaptureTests(unittest.TestCase):
    def test_original_provider_metadata_is_built_only_from_fixed_authenticated_reads(self):
        index, request, data, env = fixture(); client = FakeApi(data)
        provider, inventories = capture.capture(request, index, client, env)
        capture.validate_context(request, provider)
        self.assertEqual(len(client.calls), 6)
        self.assertEqual(len(provider["runtimeRestSnapshot"]["target_artifacts"]), 4)
        self.assertEqual(set(inventories), {"floorp", "runtime"})
        self.assertFalse(any("zip" in p for p in client.calls))

    def test_missing_local_foreign_or_retried_controller_never_reads_api(self):
        index, request, data, env = fixture()
        for key, value in [("GITHUB_ACTIONS", "false"), ("GITHUB_REPOSITORY_ID", "2"), ("GITHUB_RUN_ATTEMPT", "2"), ("GITHUB_SHA", "c" * 40), ("GITHUB_RUN_ID", "123")]:
            client = FakeApi(data)
            with self.subTest(key=key), self.assertRaises(ValueError):
                capture.capture(request, index, client, {**env, key: value})
            self.assertEqual(client.calls, [])

    def test_foreign_runtime_missing_target_or_changed_original_identity_blocks(self):
        index, request, data, env = fixture()
        changes = [
            ("/repos/Floorp-Projects/Floorp-Runtime/actions/runs/122", lambda v: v.update(run_attempt=2)),
            ("/repos/Floorp-Projects/Floorp/actions/runs/124", lambda v: v.update(status="completed")),
            ("/repos/Floorp-Projects/Floorp-Runtime/actions/runs/122/artifacts?per_page=100&page=1", lambda v: v["artifacts"][0].update(expired=True)),
            ("/repos/Floorp-Projects/Floorp/actions/runs/123/artifacts?per_page=100&page=1", lambda v: v["artifacts"][0]["workflow_run"].update(id=999)),
        ]
        for endpoint, change in changes:
            changed = copy.deepcopy(data); change(changed[endpoint])
            with self.subTest(endpoint=endpoint), self.assertRaises(ValueError):
                capture.capture(request, index, FakeApi(changed), env)

    def test_partial_duplicate_changed_or_excessive_pages_are_rejected(self):
        endpoint = "/repos/Floorp-Projects/Floorp/actions/runs/123/jobs"
        valid = {endpoint + "?per_page=100&page=1": {"total_count": 101, "jobs": [{"id": n} for n in range(1, 101)]}, endpoint + "?per_page=100&page=2": {"total_count": 101, "jobs": [{"id": 101}]}}
        self.assertEqual(len(capture.pages(FakeApi(valid), endpoint, "jobs")), 2)
        for mutate in [lambda v: v[endpoint + "?per_page=100&page=2"].update(total_count=102), lambda v: v[endpoint + "?per_page=100&page=2"]["jobs"][0].update(id=1), lambda v: v[endpoint + "?per_page=100&page=1"].update(jobs=[{"id": 1}]), lambda v: v[endpoint + "?per_page=100&page=1"].update(total_count=True)]:
            changed = copy.deepcopy(valid); mutate(changed)
            with self.assertRaises(ValueError): capture.pages(FakeApi(changed), endpoint, "jobs")

    def test_transport_never_redirects_downloads_retries_or_persists_credentials(self):
        class Response:
            status = 403
            def read1(self, _):
                raise AssertionError("403 body or redirect must never be read")
        class Connection:
            def __init__(self, *args, **kwargs): pass
            def request(self, method, endpoint, headers):
                self.asserted = (method, endpoint, headers)
            def getresponse(self): return Response()
            def close(self): pass
        with tempfile.TemporaryDirectory() as d:
            client = capture.GitHubRest("test-credential-never-persist", Path(d) / "raw")
            with patch.object(capture.http.client, "HTTPSConnection", Connection):
                with self.assertRaisesRegex(ValueError, "HTTP 403; no retry"):
                    client.get("/repos/Floorp-Projects/Floorp/actions/runs/123")
            self.assertEqual(client.count, 1)
            self.assertEqual(list(client.directory.iterdir()), [])
            for endpoint in ["https://api.github.com/repos/X", "/repos/other/repo/actions/runs/1", "/repos/Floorp-Projects/Floorp/actions/artifacts/11916387386/zip"]:
                with self.assertRaises(ValueError): client.get(endpoint)
            self.assertEqual(client.count, 1)

    def test_duplicate_json_oversize_and_request_budget_are_bounded(self):
        for data in [b'{"id":1,"id":2}', b'{"value":NaN}', b"x" * (4 * 1024**2 + 1)]:
            with self.assertRaises(ValueError): capture.parse(data)
        with tempfile.TemporaryDirectory() as d:
            client = capture.GitHubRest("test", Path(d) / "raw"); client.count = 50
            with self.assertRaises(ValueError): client.get("/repos/Floorp-Projects/Floorp/actions/runs/123")

    def test_fixed_origin_success_retains_exact_raw_bytes_and_excludes_token(self):
        raw, token = b'{"id":123}\n', "fixture-secret-only-in-header"
        response = Mock(status=200); response.read1.side_effect = io.BytesIO(raw).read1
        connection = Mock(); connection.getresponse.return_value = response
        with tempfile.TemporaryDirectory() as d, patch.object(capture.http.client, "HTTPSConnection", return_value=connection) as factory:
            client = capture.GitHubRest(token, Path(d) / "raw")
            self.assertEqual(client.get("/repos/Floorp-Projects/Floorp/actions/runs/123"), {"id": 123})
            self.assertEqual(factory.call_args.args, ("api.github.com",))
            self.assertEqual(connection.request.call_args.args, ("GET", "/repos/Floorp-Projects/Floorp/actions/runs/123"))
            self.assertEqual(connection.request.call_args.kwargs["headers"]["Authorization"], "Bearer " + token)
            self.assertEqual((client.directory / "001.json").read_bytes(), raw)
            self.assertNotIn(token, json.dumps(client.responses))
            self.assertTrue(client.responses[0]["jsonValidated"])
            self.assertTrue(client.responses[0]["completeBody"])
            connection.close.assert_called_once()

    def test_stream_byte_limits_and_timeout_retain_rejected_original_prefix(self):
        endpoint = "/repos/Floorp-Projects/Floorp/actions/runs/123"
        with tempfile.TemporaryDirectory() as d:
            for label, data, previous, expected in [("oversized", b" " * (4 * 1024**2 + 100), 0, 4 * 1024**2 + 1), ("total", b" " * 100, 32 * 1024**2 - 10, 11)]:
                response = Mock(status=200); response.read1.side_effect = io.BytesIO(data).read1
                connection = Mock(); connection.getresponse.return_value = response
                with patch.object(capture.http.client, "HTTPSConnection", return_value=connection):
                    client = capture.GitHubRest("fixture", Path(d) / label); client.bytes = previous
                    with self.assertRaises(ValueError): client.get(endpoint)
                self.assertEqual((client.directory / "001.json").stat().st_size, expected)
                self.assertFalse(client.responses[0]["completeBody"])
                self.assertFalse(client.responses[0]["jsonValidated"])
                connection.close.assert_called_once()
            response = Mock(status=200); response.read1.return_value = b"partial-original"
            connection = Mock(); connection.getresponse.return_value = response
            client = capture.GitHubRest("fixture", Path(d) / "deadline"); client.deadline = 200
            with patch.object(capture.http.client, "HTTPSConnection", return_value=connection), patch.object(capture.time, "monotonic", side_effect=[100, 100, 201]):
                with self.assertRaisesRegex(ValueError, "deadline"): client.get(endpoint)
            self.assertEqual((client.directory / "001.json").read_bytes(), b"partial-original")
            self.assertFalse(client.responses[0]["completeBody"])
            self.assertEqual(response.read1.call_count, 1)


if __name__ == "__main__": unittest.main()
