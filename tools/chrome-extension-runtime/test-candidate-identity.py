#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Source-only candidate binding rejection tests; no native runtime claims."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location("candidate_identity", Path(__file__).with_name("candidate-identity.py"))
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)
selection = identity._selection
prepare_spec = importlib.util.spec_from_file_location("candidate_prepare", Path(__file__).with_name("prepare-native-source.py"))
prepare = importlib.util.module_from_spec(prepare_spec)
prepare_spec.loader.exec_module(prepare)
ci_spec = importlib.util.spec_from_file_location("candidate_ci", Path(__file__).parent / "ci/test-candidate.py")
ci = importlib.util.module_from_spec(ci_spec)
ci_spec.loader.exec_module(ci)



class CandidatePackageSelectionTests(unittest.TestCase):
    def test_windows_product_and_auxiliary_zip_select_only_the_application(self):
        product = {"name": "floorp-158.0.1.en-US.win64.zip", "sha256": "product"}
        auxiliary = {"name": "floorp-158.0.1.en-US.win64.xpt_artifacts.zip", "sha256": "typelibs"}
        self.assertEqual(selection.select_native_package([auxiliary, product], "windows"), product)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for entry in [product, auxiliary]:
                (root / entry["name"]).write_bytes(b"authored selection fixture, never executed")
            self.assertEqual(selection.distributable_packages(root), [root / product["name"]])

    def test_multiple_real_product_zips_remain_ambiguous(self):
        entries = [{"name": name} for name in
                   ["floorp-one.zip", "floorp-two.zip", "floorp-one.xpt_artifacts.zip"]]
        with self.assertRaisesRegex(RuntimeError, "exactly one native product"):
            selection.select_native_package(entries, "windows")
        self.assertTrue(selection.is_product_archive("floorp-other-artifact.zip"))
        self.assertTrue(selection.is_product_archive("floorp-xpt_artifacts.zip"))

    def test_auxiliary_zip_cannot_supply_a_missing_product(self):
        with self.assertRaisesRegex(RuntimeError, "exactly one native product"):
            selection.select_native_package([{"name": "floorp-one.xpt_artifacts.zip"}], "windows")


class CandidateIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="floorp-identity-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        subprocess.run(["git", "init", "-q", self.root], check=True)
        subprocess.run(["git", "-C", self.root, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                        "commit", "--allow-empty", "-qm", "Identity test fixture"], check=True)
        commit = subprocess.check_output(["git", "-C", self.root, "rev-parse", "HEAD"], text=True).strip()
        self.patch = self.root / ".github/patches/floorp-runtime/common/chrome-fixture.patch"
        self.patch.parent.mkdir(parents=True)
        self.patch.write_text("authored fixture patch\n")
        self.baseline_patch = self.patch.with_name("browser-modules-BrowserGlue.sys.patch")
        self.baseline_patch.write_text("authored production startup baseline\n")
        self.windows_patch = self.root / ".github/patches/floorp-runtime/windows/custom-app-icons-branding.windows.patch"
        self.windows_patch.parent.mkdir()
        self.windows_patch.write_text("authored Windows branding baseline\n")
        (self.root / "floorp-runtime.lock.json").write_text(json.dumps({
            "source": {"commit": "a" * 40}, "artifacts": [{"platform": "linux", "version": "158.0.1"}]}))
        self.app = self.root / "installed/floorp"
        self.app.mkdir(parents=True)
        self.files = {"floorp": b"AUTHORED EXECUTABLE PLACEHOLDER; NOT RUN", "libxul.so": b"authored native-library fixture",
                      "application.ini": b"[App]\nVersion=158.0.1\nBuildID=20261019000000\n", "omni.ja": b"authored resources"}
        for name, data in self.files.items():
            (self.app / name).write_bytes(data)
        self.package = self.root / "floorp-fixture.tar.gz"
        self.write_tar()
        self.manifest = {"schemaVersion": 1, "platform": "linux", "sourceCommit": "a" * 40,
                         "floorpCommit": commit, "productionOverlay": True, "nativeWebExtensionBindings": True,
                         "buildId": "20261019000000", "patches": {self.patch.name: identity._sha(self.patch),
                         self.baseline_patch.name: identity._sha(self.baseline_patch)}}
        self.manifest_path = self.root / "candidate-manifest.json"
        self.write_manifest()
        host = patch.object(identity.sys, "platform", "linux")
        host.start()
        self.addCleanup(host.stop)

    def write_tar(self, extra=None):
        with tarfile.open(self.package, "w:gz") as archive:
            for name, data in {**self.files, **(extra or {})}.items():
                member = tarfile.TarInfo("floorp/" + name)
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))

    def write_manifest(self):
        self.manifest["packages"] = [{"name": self.package.name, "bytes": self.package.stat().st_size,
                                      "sha256": identity._sha(self.package)}]
        self.manifest_path.write_text(json.dumps(self.manifest))

    def authenticate(self):
        return identity.authenticate_candidate(self.manifest_path, self.app / "floorp", self.root)

    def test_all_regular_files_bound_and_modified_native_library_rejected(self):
        result = self.authenticate()
        self.assertEqual(result["verifiedFileCount"], 4)
        self.assertEqual(result["binarySha256"], hashlib.sha256(self.files["floorp"]).hexdigest())
        (self.app / "libxul.so").write_bytes(b"different native code")
        with self.assertRaisesRegex(RuntimeError, "Installed native package differs"):
            self.authenticate()

    def test_extra_injected_file_rejected(self):
        (self.app / "injected.js").write_text("authored extra file")
        with self.assertRaisesRegex(RuntimeError, "extra="):
            self.authenticate()

    def test_manifest_cannot_bind_another_installed_tree(self):
        other = self.root / "other"
        other.mkdir()
        (other / "floorp").write_bytes(self.files["floorp"])
        with self.assertRaisesRegex(RuntimeError, "Installed native package differs"):
            identity.authenticate_candidate(self.manifest_path, other / "floorp", self.root)

    def test_package_bytes_and_current_patch_are_checked(self):
        self.package.write_bytes(self.package.read_bytes() + b"changed package")
        with self.assertRaisesRegex(RuntimeError, "checksum or size"):
            self.authenticate()
        self.write_manifest()
        self.patch.write_text("changed maintained source")
        with self.assertRaisesRegex(RuntimeError, "maintained source"):
            self.authenticate()

    def test_non_chrome_baseline_omission_and_change_rejected_by_both_consumers(self):
        label = self.baseline_patch.name
        correct = self.manifest["patches"][label]
        for value in (None, "0" * 64):
            with self.subTest(recorded=value):
                if value is None:
                    self.manifest["patches"].pop(label, None)
                else:
                    self.manifest["patches"][label] = value
                self.write_manifest()
                with self.assertRaisesRegex(RuntimeError, label):
                    self.authenticate()
                with patch.object(ci, "ROOT", self.root), patch.object(ci.sys, "argv", [
                        "test-candidate.py", "--platform", "linux", "--artifact", str(self.root)]), \
                        patch.object(ci, "unpack") as unpack:
                    with self.assertRaisesRegex(RuntimeError, label):
                        ci.main()
                    unpack.assert_not_called()
                self.manifest["patches"][label] = correct

    def test_windows_patch_omission_and_change_rejected_by_both_consumers(self):
        self.manifest["platform"] = "windows"
        label = "windows/" + self.windows_patch.name
        # A bare filename cannot impersonate the production selector's OS label.
        self.manifest["patches"][self.windows_patch.name] = identity._sha(self.windows_patch)
        for value in (None, "0" * 64):
            with self.subTest(recorded=value), patch.object(identity.sys, "platform", "win32"):
                if value is None:
                    self.manifest["patches"].pop(label, None)
                else:
                    self.manifest["patches"][label] = value
                self.write_manifest()
                with self.assertRaisesRegex(RuntimeError, label):
                    self.authenticate()
                with patch.object(ci, "ROOT", self.root), patch.object(ci.sys, "argv", [
                        "test-candidate.py", "--platform", "windows", "--artifact", str(self.root)]), \
                        patch.object(ci, "unpack") as unpack:
                    with self.assertRaisesRegex(RuntimeError, label):
                        ci.main()
                    unpack.assert_not_called()

    def select_inherited_production_patches(self, platform="linux"):
        original = self.root / "locked-runtime-fixture"
        upstream = original / ".github/patches/upstream"
        upstream.mkdir(parents=True)
        # Exercise the actual production selector's byte-identical deduplication.
        (upstream / self.baseline_patch.name).write_bytes(self.baseline_patch.read_bytes())
        selected = prepare.select_patches(original, self.patch.parent, True, platform)
        self.manifest["platform"] = platform
        self.manifest["patches"] = {label: identity._sha(path) for label, path in selected}
        return "runtime-upstream/" + self.baseline_patch.name

    def assert_ci_reaches_extraction(self):
        # Successful preflight must reach extraction; never execute the authored
        # executable placeholder or pretend this is a native browser result.
        with patch.object(ci, "ROOT", self.root), patch.object(ci.sys, "argv", [
                "test-candidate.py", "--platform", "linux", "--artifact", str(self.root)]), \
                patch.object(ci, "unpack", side_effect=RuntimeError("authored extraction boundary")) as unpack:
            with self.assertRaisesRegex(RuntimeError, "authored extraction boundary"):
                ci.main()
            unpack.assert_called_once()

    def assert_patch_rejected_by_both_consumers(self, label, platform="linux"):
        with self.assertRaisesRegex(RuntimeError, label):
            self.authenticate()
        with patch.object(ci, "ROOT", self.root), patch.object(ci.sys, "argv", [
                "test-candidate.py", "--platform", platform, "--artifact", str(self.root)]), \
                patch.object(ci, "unpack") as unpack:
            with self.assertRaisesRegex(RuntimeError, label):
                ci.main()
            unpack.assert_not_called()

    def test_actual_production_selector_inherited_common_passes_both_consumers(self):
        alias = self.select_inherited_production_patches()
        self.assertNotIn(self.baseline_patch.name, self.manifest["patches"])
        self.assertEqual(self.manifest["patches"][alias], identity._sha(self.baseline_patch))
        self.write_manifest()
        self.assertEqual(self.authenticate()["verifiedFileCount"], 4)
        self.assert_ci_reaches_extraction()
        # The identical common inheritance rule applies on every production OS.
        for platform in ("linux", "mac", "windows"):
            original = self.root / "locked-runtime-fixture"
            selected = prepare.select_patches(original, self.patch.parent, True, platform)
            manifest = {"platform": platform, "patches": {label: identity._sha(path) for label, path in selected}}
            identity.validate_maintained_patches(manifest, self.root)

    def test_inherited_common_missing_or_wrong_digest_rejected_before_extraction(self):
        alias = self.select_inherited_production_patches()
        for digest in (None, "0" * 64):
            with self.subTest(aliasDigest=digest):
                if digest is None:
                    self.manifest["patches"].pop(alias, None)
                else:
                    self.manifest["patches"][alias] = digest
                self.write_manifest()
                self.assert_patch_rejected_by_both_consumers(self.baseline_patch.name)

    def test_conflicting_direct_common_label_cannot_hide_behind_valid_inheritance(self):
        alias = self.select_inherited_production_patches()
        self.assertEqual(self.manifest["patches"][alias], identity._sha(self.baseline_patch))
        for digest in (None, "0" * 64):
            with self.subTest(directDigest=digest):
                self.manifest["patches"][self.baseline_patch.name] = digest
                self.write_manifest()
                self.assert_patch_rejected_by_both_consumers(self.baseline_patch.name)

    def test_windows_source_cannot_inherit_a_common_upstream_alias(self):
        self.select_inherited_production_patches("windows")
        label = "windows/" + self.windows_patch.name
        digest = self.manifest["patches"].pop(label)
        self.manifest["patches"]["runtime-upstream/" + self.windows_patch.name] = digest
        self.write_manifest()
        with patch.object(identity.sys, "platform", "win32"):
            self.assert_patch_rejected_by_both_consumers(label, "windows")

    def test_wrong_source_commit_and_false_native_marker_rejected(self):
        for key, value in (("sourceCommit", "b" * 40), ("floorpCommit", "c" * 40),
                           ("productionOverlay", False), ("nativeWebExtensionBindings", False)):
            previous = self.manifest[key]
            self.manifest[key] = value
            self.write_manifest()
            with self.assertRaisesRegex(RuntimeError, "native production source"):
                self.authenticate()
            self.manifest[key] = previous

    def test_application_ini_and_running_runtime_are_separate_gates(self):
        candidate = self.authenticate()
        runtime = {"version": "158.0.1", "buildId": "20261019000000", "os": "Linux",
                   "abi": "x86_64-gcc3", "workerWebIDLCompiled": True}
        identity.verify_runtime_identity(candidate, runtime)
        for key, value in (("version", "157.0.1"), ("buildId", "20200101000000"), ("os", "WINNT"),
                           ("abi", "aarch64-gcc3"), ("workerWebIDLCompiled", False)):
            with self.assertRaisesRegex(RuntimeError, "Running browser"):
                identity.verify_runtime_identity(candidate, {**runtime, key: value})
        self.files["application.ini"] = b"[App]\nVersion=157.0.1\nBuildID=20261019000000\n"
        (self.app / "application.ini").write_bytes(self.files["application.ini"])
        self.write_tar()
        self.write_manifest()
        with self.assertRaisesRegex(RuntimeError, "application.ini"):
            self.authenticate()

    def test_zip_inventory_and_path_traversal_rejection(self):
        package = self.root / "authored-windows-fixture.zip"
        with zipfile.ZipFile(package, "w") as archive:
            for name, data in self.files.items():
                archive.writestr("floorp/" + name, data)
        (count, _), _ = identity._archive_inventory(package, self.app / "floorp")
        self.assertEqual(count, 4)
        with zipfile.ZipFile(package, "a") as archive:
            archive.writestr("../outside", b"must never be extracted")
        with self.assertRaisesRegex(ValueError, "Unsafe native archive path"):
            identity._archive_inventory(package, self.app / "floorp")

    def test_windows_authentication_with_typelib_zip_and_ambiguous_products(self):
        self.package = self.root / "floorp-fixture.zip"
        with zipfile.ZipFile(self.package, "w") as archive:
            for name, data in self.files.items():
                archive.writestr("floorp/" + name, data)
        lock_path = self.root / "floorp-runtime.lock.json"
        lock = json.loads(lock_path.read_text())
        lock["artifacts"][0]["platform"] = "windows"
        lock_path.write_text(json.dumps(lock))
        self.manifest["platform"] = "windows"
        self.manifest["patches"]["windows/" + self.windows_patch.name] = identity._sha(self.windows_patch)
        self.write_manifest()
        self.manifest["packages"].append({"name": "floorp-fixture.xpt_artifacts.zip"})
        self.manifest_path.write_text(json.dumps(self.manifest))
        with patch.object(identity.sys, "platform", "win32"):
            self.assertEqual(self.authenticate()["verifiedFileCount"], 4)
            self.manifest["packages"].append({"name": "floorp-other-product.zip"})
            self.manifest_path.write_text(json.dumps(self.manifest))
            with self.assertRaisesRegex(RuntimeError, "exactly one native product"):
                self.authenticate()

    def test_symlink_escape_rejected(self):
        if not hasattr(Path, "symlink_to"):
            self.skipTest("Host has no symlink API")
        outside = self.root / "outside"
        outside.write_bytes(b"outside application")
        (self.app / "link").symlink_to(outside)
        with self.assertRaisesRegex(ValueError, "symlink escapes"):
            self.authenticate()


class CandidatePatchInventoryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="floorp-patch-inventory-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.folder = self.root / ".github/patches/floorp-runtime"
        (self.folder / "common").mkdir(parents=True)
        (self.folder / "windows").mkdir()
        self.common = self.folder / "common/browser-init.patch"
        self.common.write_text("authored startup patch\n")
        self.windows = self.folder / "windows/custom-app-icons.windows.patch"
        self.windows.write_text("authored OS patch\n")
        self.manifest = {"platform": "linux", "patches": {
            self.common.name: identity._sha(self.common),
            "runtime-upstream/extra.patch": "upstream descriptor remains allowed",
            "runtime-packaging/extra.patch": "packaging descriptor remains allowed"}}

    def test_common_inventory_accepts_extra_authenticated_namespaces_without_runtime_clone(self):
        identity.validate_maintained_patches(self.manifest, self.root)
        self.common.write_text("altered checkout startup patch\n")
        with self.assertRaisesRegex(RuntimeError, self.common.name):
            identity.validate_maintained_patches(self.manifest, self.root)

    def test_linux_and_mac_do_not_require_windows_records_or_folder(self):
        for platform in ("linux", "mac"):
            self.manifest["platform"] = platform
            identity.validate_maintained_patches(self.manifest, self.root)
        self.windows.unlink()
        self.windows.parent.rmdir()
        for platform in ("linux", "mac"):
            self.manifest["platform"] = platform
            identity.validate_maintained_patches(self.manifest, self.root)

    def test_windows_requires_namespaced_bytes_and_nonempty_os_inventory(self):
        self.manifest["platform"] = "windows"
        self.manifest["patches"]["windows/" + self.windows.name] = identity._sha(self.windows)
        identity.validate_maintained_patches(self.manifest, self.root)
        self.windows.write_text("changed OS source\n")
        with self.assertRaisesRegex(RuntimeError, "windows/" + self.windows.name):
            identity.validate_maintained_patches(self.manifest, self.root)
        self.windows.unlink()
        with self.assertRaisesRegex(RuntimeError, "inventory is empty: windows"):
            identity.validate_maintained_patches(self.manifest, self.root)
        self.windows.parent.rmdir()
        with self.assertRaisesRegex(RuntimeError, "inventory is empty: windows"):
            identity.validate_maintained_patches(self.manifest, self.root)

    def test_missing_or_empty_common_inventory_and_malformed_map_fail_closed(self):
        for recorded in (None, [], "not a patch map"):
            with self.subTest(patches=recorded), self.assertRaisesRegex(RuntimeError, "inventory must be an object"):
                identity.validate_maintained_patches({"platform": "linux", "patches": recorded}, self.root)
        self.common.unlink()
        with self.assertRaisesRegex(RuntimeError, "inventory is empty: common"):
            identity.validate_maintained_patches(self.manifest, self.root)
        self.common.parent.rmdir()
        with self.assertRaisesRegex(RuntimeError, "inventory is empty: common"):
            identity.validate_maintained_patches(self.manifest, self.root)

    def test_unknown_platform_is_not_an_implicit_non_windows_selection(self):
        with self.assertRaisesRegex(RuntimeError, "supported platform"):
            identity.validate_maintained_patches({**self.manifest, "platform": "win32"}, self.root)


if __name__ == "__main__":
    unittest.main()
