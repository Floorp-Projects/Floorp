import plistlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import validate_macos_helper_trust as v


class HelperTrustTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.app = Path(self.tmp.name) / "Floorp.app"
        self.appbin = self.app / "Contents/MacOS/floorp"
        self.upapp = self.app / "Contents/MacOS/updater.app"
        self.up = self.upapp / "Contents/MacOS" / v.UPDATER
        self.launch = self.app / "Contents/Library/LaunchServices" / v.UPDATER
        for f in (self.appbin, self.up, self.launch):
            f.parent.mkdir(parents=True, exist_ok=True)
            f.touch()
        self.info = {
            "CFBundleExecutable": "floorp",
            "CFBundleIdentifier": "app.floorp.Floorp",
            "SMPrivilegedExecutables": {v.UPDATER: v.production_requirement(v.UPDATER)},
        }
        self.write_info()
        self.reject = None
        self.arch_mismatch = None
        self.invalid_req = None
        self.calls = []
        self.runner = patch.object(v, "run", side_effect=self.fake_run)
        self.runner.start()
        self.addCleanup(self.runner.stop)
        self.embed_patch = patch.object(
            v,
            "embedded_plist",
            return_value={
                "SMAuthorizedClients": [v.production_requirement("app.floorp.Floorp")]
            },
        )
        self.embed = self.embed_patch.start()
        self.addCleanup(self.embed_patch.stop)

    def write_info(self):
        with (self.app / "Contents/Info.plist").open("wb") as f:
            plistlib.dump(self.info, f)

    def fake_run(self, *args):
        self.calls.append(args)
        if args[0].endswith("lipo"):
            return (
                "arm64"
                if self.arch_mismatch and args[-1] == str(self.arch_mismatch)
                else "arm64 x86_64"
            )
        if args[0].endswith("csreq") and args[2] == "=" + str(self.invalid_req):
            raise v.ValidationError("invalid requirement")
        if args[0].endswith("csreq"):
            return args[2][1:]
        if (
            self.reject
            and args[0].endswith("codesign")
            and any(a.startswith("-R=") for a in args)
            and self.reject(args)
        ):
            raise v.ValidationError("requirement mismatch")
        return ""

    def test_valid_checks_both_copies_both_architectures(self):
        v.validate(self.app, "app.floorp.Floorp")
        checks = [x for x in self.calls if any(a.startswith("-R=") for a in x)]
        self.assertEqual(len(checks), 14)
        self.assertEqual(
            {x[x.index("--architecture") + 1] for x in checks}, {"arm64", "x86_64"}
        )

    def test_missing_helper_fails(self):
        self.launch.unlink()
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_missing_privileged_requirement_fails(self):
        self.info["SMPrivilegedExecutables"] = {}
        self.write_info()
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_missing_authorized_clients_fails(self):
        self.embed.return_value = {}
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_malformed_requirement_fails(self):
        self.invalid_req = v.production_requirement("app.floorp.Floorp")
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_helper_to_app_mismatch_fails(self):
        self.reject = lambda args: args[-1] == str(self.app)
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_app_to_second_helper_mismatch_fails(self):
        self.reject = lambda args: args[-1] == str(self.launch)
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_multislice_mismatch_fails(self):
        self.arch_mismatch = self.launch
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_single_slice_requirement_mismatch_fails(self):
        self.reject = lambda args: "x86_64" in args and args[-1] == str(self.app)
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_dmg_helper_if_present_is_also_checked(self):
        dmg = self.app / "Contents/Library/LaunchServices" / v.DMG_HELPER
        dmg.touch()
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")
        self.info["SMPrivilegedExecutables"][v.DMG_HELPER] = v.production_requirement(
            v.DMG_HELPER
        )
        self.write_info()
        v.validate(self.app, "app.floorp.Floorp")

    def test_relaxed_embedded_requirement_fails_even_if_matches(self):
        self.embed.return_value = {"SMAuthorizedClients": ["anchor apple generic"]}
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_self_consistent_wrong_bundle_id_fails(self):
        self.info["CFBundleIdentifier"] = "app.floorp.Wrong"
        self.write_info()
        self.embed.return_value = {
            "SMAuthorizedClients": [v.production_requirement("app.floorp.Wrong")]
        }
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")

    def test_bad_app_plist_fails(self):
        (self.app / "Contents/Info.plist").write_bytes(b"not plist")
        with self.assertRaises(v.ValidationError):
            v.validate(self.app, "app.floorp.Floorp")


class OtoolParsingTest(unittest.TestCase):
    def test_x86_bytes_and_arm_words(self):
        data = plistlib.dumps({"SMAuthorizedClients": ["x"]}, fmt=plistlib.FMT_XML)
        for arch in ("x86_64", "arm64"):
            lines = []
            for start in range(0, len(data), 16):
                chunk = data[start : start + 16]
                fields = []
                while chunk:
                    n = 4 if arch == "arm64" and len(chunk) >= 4 else 1
                    part, chunk = chunk[:n], chunk[n:]
                    if n == 4:
                        part = part[::-1]
                    fields.append(part.hex())
                lines.append(f"{0x100000000+start:016x} " + " ".join(fields))
            with patch.object(v, "run", return_value="\n".join(lines)):
                self.assertEqual(
                    v.embedded_plist(Path("helper"), arch)["SMAuthorizedClients"], ["x"]
                )

    def test_invalid_dump_rejected(self):
        with patch.object(v, "run", return_value="0000000100000000 nothex"):
            with self.assertRaises(v.ValidationError):
                v.embedded_plist(Path("helper"), "arm64")


if __name__ == "__main__":
    unittest.main()
