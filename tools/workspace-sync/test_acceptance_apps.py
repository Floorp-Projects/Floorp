# SPDX-License-Identifier: MPL-2.0
"""Run with: python3 -m unittest discover -s tools/workspace-sync -p 'test_*.py'."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from acceptance_app_support import (
    LIFECYCLE_RESOURCES, SERVICE_RESOURCE, app_layout, certificate_files,
    generated_entries, overlay_app, profile_preferences, proxy_preferences,
    validate_source_app, write_preferences, _patched_resources,
)

SCRIPT_DIR = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("launch_acceptance", SCRIPT_DIR / "launch-acceptance-apps.py")
LAUNCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(LAUNCH)
REFRESH_SPEC = importlib.util.spec_from_file_location("refresh_acceptance", SCRIPT_DIR / "refresh-acceptance-apps.py")
REFRESH = importlib.util.module_from_spec(REFRESH_SPEC)
REFRESH_SPEC.loader.exec_module(REFRESH)


class AcceptanceAppTests(unittest.TestCase):
    def test_proxy_preserves_https_and_loopback_routes(self):
        prefs = proxy_preferences("http://proxy:8080")
        self.assertEqual(prefs["network.proxy.ssl"], "proxy")
        self.assertEqual(prefs["network.proxy.ssl_port"], 8080)
        self.assertIn("127.0.0.1", prefs["network.proxy.no_proxies_on"])
        self.assertFalse(prefs["network.proxy.allow_hijacking_localhost"])
        self.assertNotIn("security.tls.version.min", prefs)

    def test_proxy_rejects_credentials_without_echoing_them(self):
        for value in ("http://user:secret@proxy:8080", "https://proxy:8080", "http://proxy/path"):
            with self.assertRaises(ValueError) as raised:
                proxy_preferences(value)
            self.assertNotIn("secret", str(raised.exception))

    def test_generated_entries_reject_escape_and_duplicate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "source.js").write_text("export {}")
            (root / "jar.mn").write_text("  ../escape.js (source.js)\n")
            with self.assertRaises(ValueError):
                generated_entries(root, "chrome/noraneko/")
            (root / "jar.mn").write_text("  code.js (source.js)\n  code.js (source.js)\n")
            with self.assertRaises(ValueError):
                generated_entries(root, "chrome/noraneko/")

    def test_generated_entries_refuse_symlink_outside_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            (root / "outside.js").write_text("sensitive")
            (build / "link.js").symlink_to(root / "outside.js")
            (build / "jar.mn").write_text("  code.js (link.js)\n")
            with self.assertRaises(ValueError):
                generated_entries(build, "chrome/noraneko/")

    def test_ca_inputs_require_only_one_public_certificate(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ca.pem"
            path.write_text("-----BEGIN CERTIFICATE-----\nx\n-----END CERTIFICATE-----\n-----BEGIN PRIVATE KEY-----\n")
            with self.assertRaises(ValueError):
                certificate_files([path])

    def test_overlay_changes_both_archives_and_preserves_unrelated_resources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            resources = root / "app"
            (resources / "browser").mkdir(parents=True)
            for archive in ("omni.ja", "browser/omni.ja"):
                with zipfile.ZipFile(resources / archive, "w") as zipped:
                    zipped.writestr("unrelated.txt", "keep")
                    if archive == "omni.ja":
                        zipped.writestr("chrome/noraneko/nora-resource/stale.js", "old")
                        zipped.writestr("chrome/noraneko/nora-content-modal-child/assets/old.js", "old dialog")
            patch_file = root / ".github/patches/floorp-runtime/common/workspace-container-lifecycle.patch"
            patch_file.parent.mkdir(parents=True)
            patch_file.write_text("fixture")
            service = {("omni.ja", SERVICE_RESOURCE): b"new service"}
            lifecycle = {(archive, entry): b"new lifecycle" for archive, entry in LIFECYCLE_RESOURCES.values()}
            generated = {"chrome/noraneko/nora-resource/new.js": b"new module",
                         "chrome/noraneko/nora-content-modal-child/index.html": b"new dialog"}
            with patch("acceptance_app_support._patched_resources", side_effect=[service, lifecycle]):
                hashes = overlay_app(resources, root, generated, root)
            for archive in ("omni.ja", "browser/omni.ja"):
                with zipfile.ZipFile(resources / archive) as zipped:
                    self.assertEqual(zipped.read("unrelated.txt"), b"keep")
                    self.assertNotIn("chrome/noraneko/nora-resource/stale.js", zipped.namelist())
                    self.assertNotIn("chrome/noraneko/nora-content-modal-child/assets/old.js", zipped.namelist())
                    for (target, entry), content in lifecycle.items():
                        if target == archive:
                            self.assertEqual(zipped.read(entry), content)
            self.assertEqual(len(hashes), 7)

    def test_native_patch_really_applies_inside_parent_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "--quiet", str(root)], check=True)
            scratch = root / "_dist/qa"
            scratch.mkdir(parents=True)
            with zipfile.ZipFile(scratch / "omni.ja", "w") as archive:
                archive.writestr("modules/service.js", "const patched = false;\n")
            patch_file = root / "change.patch"
            patch_file.write_text("diff --git a/modules/service.js b/modules/service.js\n"
                                  "--- a/modules/service.js\n+++ b/modules/service.js\n"
                                  "@@ -1 +1 @@\n-const patched = false;\n+const patched = true;\n")
            result = _patched_resources(scratch, patch_file,
                                        {"modules/service.js": ("omni.ja", "modules/service.js")}, scratch)
            self.assertEqual(result[("omni.ja", "modules/service.js")], b"const patched = true;\n")

    def test_overlay_applies_packaged_default_preferences(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            resources = root / "app"
            (resources / "browser").mkdir(parents=True)
            with zipfile.ZipFile(resources / "omni.ja", "w"):
                pass
            with zipfile.ZipFile(resources / "browser/omni.ja", "w") as archive:
                archive.writestr("defaults/preferences/firefox.js", "pref('old', true);\n")
            override = root / "static/gecko/pref/override.sh"
            override.parent.mkdir(parents=True)
            override.write_text("#!/bin/sh\nprintf \"pref('capability', true);\\n\" >> \"$1\"\n")
            with patch("acceptance_app_support._patched_resources", return_value={}):
                hashes = overlay_app(resources, root, {"chrome/noraneko/nora-resource/new.js": b"module"}, root)
            with zipfile.ZipFile(resources / "browser/omni.ja") as archive:
                self.assertEqual(archive.read("defaults/preferences/firefox.js"),
                                 b"pref('old', true);\npref('capability', true);\n")
            self.assertIn("browser/omni.ja:defaults/preferences/firefox.js", hashes)

    def test_native_patch_upgrade_requires_exact_previous_patch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with zipfile.ZipFile(root / "omni.ja", "w") as archive:
                archive.writestr("modules/service.js", "const version = 1;\n")
            header = "diff --git a/modules/service.js b/modules/service.js\n--- a/modules/service.js\n+++ b/modules/service.js\n@@ -1 +1 @@\n"
            previous = (header + "-const version = 0;\n+const version = 1;\n").encode()
            patch_file = root / "current.patch"
            patch_file.write_text(header + "-const version = 0;\n+const version = 2;\n")
            mapping = {"modules/service.js": ("omni.ja", "modules/service.js")}
            with self.assertRaises(ValueError):
                _patched_resources(root, patch_file, mapping, root)
            result = _patched_resources(root, patch_file, mapping, root, previous)
            self.assertEqual(result[("omni.ja", "modules/service.js")], b"const version = 2;\n")

    def make_linux_setup(self, root):
        clients = []
        for index, name in enumerate(("A", "B")):
            profile = root / f"profile-{name.lower()}"
            app = root / f"floorp-qa-{name.lower()}"
            profile.mkdir()
            app.mkdir()
            binary = app / "floorp"
            binary.write_text("#!/bin/sh\nexit 0\n")
            binary.chmod(0o700)
            (app / "workspace-sync-qa.json").write_text(json.dumps({
                "schemaVersion": 1, "client": name,
                "identity": f"org.floorp.workspace-sync.qa.{name.lower()}",
            }))
            port = 28000 + index
            write_preferences(profile, profile_preferences(name, port, {}))
            clients.append({"name": name, "profile": str(profile), "app": str(app),
                            "binary": str(binary), "marionettePort": port})
        return {"platform": "linux", "clients": clients}

    def test_launcher_accepts_two_linux_profiles_with_distinct_ports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            setup = self.make_linux_setup(root)
            self.assertEqual(len(LAUNCH.validate_setup(root / "setup.json", setup)), 2)
            self.assertEqual(len(LAUNCH.validate_setup(root / "setup.json", setup, "A")), 1)

    def test_launcher_rejects_port_collision_and_external_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            setup = self.make_linux_setup(root)
            setup["clients"][1]["marionettePort"] = setup["clients"][0]["marionettePort"]
            with self.assertRaises(ValueError):
                LAUNCH.validate_setup(root / "setup.json", setup)
            setup["clients"][1]["marionettePort"] += 1
            setup["clients"][0]["profile"] = str(root)
            with self.assertRaises(ValueError):
                LAUNCH.validate_setup(root / "setup.json", setup)

    def test_launcher_refuses_missing_trust_or_modified_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            setup = self.make_linux_setup(root)
            setup["clients"][0]["caCertificateSha256"] = ["expected"]
            with self.assertRaises(ValueError):
                LAUNCH.validate_setup(root / "setup.json", setup)
            del setup["clients"][0]["caCertificateSha256"]
            (root / "floorp-qa-a/workspace-sync-qa.json").write_text("{}")
            with self.assertRaises(ValueError):
                LAUNCH.validate_setup(root / "setup.json", setup)

    def test_source_validation_requires_full_app(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                validate_source_app(Path(directory), "linux")
            self.assertEqual(app_layout(Path(directory), "macos")[1],
                             Path(directory) / "Contents/MacOS/floorp")

    def refresh_fixture(self, root):
        original = self.make_linux_setup(root)
        base = root / "floorp-qa-base"
        base.mkdir()
        original["app"] = str(base)
        for client in original["clients"]:
            (Path(client["profile"]) / "preserved-state").write_text("fixture state")
        stage = root / "stage"
        stage.mkdir()
        prepared = self.make_linux_setup(stage)
        stage_base = stage / "floorp-qa-base"
        stage_base.mkdir()
        prepared["app"] = str(stage_base)
        for key in ("buildCommit", "buildTree", "engineSha256", "archiveSha256", "browserArchiveSha256"):
            prepared[key] = "new"
        prepared.update(buildWorkingTreeDirty=True, resourceSha256={"resource": "new"})
        for client in prepared["clients"]:
            client.update(archiveSha256="new", browserArchiveSha256="new")
            (Path(client["app"]) / "new-build").write_text("updated")
        backup = root / "backup"
        backup.mkdir()
        setup_file = root / "setup.json"
        setup_file.write_text(json.dumps(original))
        return setup_file, original, prepared, backup

    def test_refresh_replaces_apps_and_preserves_profile_paths_ports_and_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            setup_file, original, prepared, backup = self.refresh_fixture(root)
            REFRESH.replace_apps(setup_file, original, prepared, backup)
            updated = json.loads(setup_file.read_text())
            for old, client in zip(original["clients"], updated["clients"]):
                self.assertEqual(client["profile"], old["profile"])
                self.assertEqual(client["marionettePort"], old["marionettePort"])
                self.assertEqual((Path(client["profile"]) / "preserved-state").read_text(), "fixture state")
                self.assertEqual((Path(client["app"]) / "new-build").read_text(), "updated")
            self.assertEqual(updated["engineSha256"], "new")

    def test_failed_refresh_rolls_back_previously_replaced_apps(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            setup_file, original, prepared, backup = self.refresh_fixture(root)
            failed_replacement = Path(prepared["clients"][1]["app"])
            real_rename = Path.rename

            def rename(path, target):
                if path == failed_replacement:
                    raise OSError("fixture rename failure")
                return real_rename(path, target)

            with patch.object(Path, "rename", rename), self.assertRaises(OSError):
                REFRESH.replace_apps(setup_file, original, prepared, backup)
            self.assertEqual(json.loads(setup_file.read_text()), original)
            for client in original["clients"]:
                self.assertFalse((Path(client["app"]) / "new-build").exists())
                self.assertEqual((Path(client["profile"]) / "preserved-state").read_text(), "fixture state")


if __name__ == "__main__":
    unittest.main()
