import hashlib
import json
import lzma
import struct
import tempfile
import unittest
from pathlib import Path

from finalize_macos_update_metadata import FILES, finalize


class MetadataTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.app = self.root / "Floorp.app"
        self.mar = self.root / "full.mar"
        self.meta = self.root / "meta.json"
        self.contents = {
            FILES[0]: b"[App]\nVersion=12.21.1@159.0.1\nBuildID=20261105161203\n",
            FILES[1]: b"[Build]\nBuildID=20261105161203\nMilestone=159.0.1\n",
            FILES[2]: b"packaged-build-id",
        }
        for name, content in self.contents.items():
            path = self.app / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.metadata = {
            "version_display": "12.21.1@159.0.1",
            "version": "159.0.1",
            "noraneko_buildid": "packaged-build-id",
            "buildid": "20261105124610",
            "mar_size": 0,
            "mar_shasum": "stale",
            "channel": "release",
        }
        self.meta.write_text(json.dumps(self.metadata))
        self.write_mar(self.contents)

    def write_mar(self, contents):
        data = bytearray(b"MAR1" + b"\0" * 4)
        index = bytearray()
        for name, content in contents.items():
            packed = lzma.compress(content)
            index.extend(
                struct.pack(">III", len(data), len(packed), 0) + name.encode() + b"\0"
            )
            data.extend(packed)
        struct.pack_into(">I", data, 4, len(data))
        data.extend(struct.pack(">I", len(index)) + index)
        self.mar.write_bytes(data)

    def test_stale_metadata_replaced_by_package_and_mar_checked(self):
        actual = finalize(self.app, self.mar, self.meta)
        self.assertEqual(actual["buildid"], "20261105161203")
        self.assertEqual(actual["mar_size"], self.mar.stat().st_size)
        self.assertEqual(
            actual["mar_shasum"], hashlib.sha512(self.mar.read_bytes()).hexdigest()
        )
        self.assertEqual(actual["channel"], "release")

    def test_reject_mismatched_mar(self):
        contents = self.contents.copy()
        contents[FILES[0]] = contents[FILES[0]].replace(
            b"20261105161203", b"20261105124610"
        )
        self.write_mar(contents)
        with self.assertRaisesRegex(ValueError, "signed app and MAR differ"):
            finalize(self.app, self.mar, self.meta)

    def test_reject_mismatched_packaged_ini_buildids(self):
        contents = self.contents.copy()
        contents[FILES[1]] = contents[FILES[1]].replace(
            b"20261105161203", b"20261105124610"
        )
        (self.app / FILES[1]).write_bytes(contents[FILES[1]])
        self.write_mar(contents)
        with self.assertRaisesRegex(ValueError, "inconsistent packaged BuildID"):
            finalize(self.app, self.mar, self.meta)

    def test_reject_missing_payload_file(self):
        self.write_mar({k: v for k, v in self.contents.items() if k != FILES[1]})
        with self.assertRaisesRegex(ValueError, "missing required MAR entries"):
            finalize(self.app, self.mar, self.meta)

    def test_reject_version_mismatch(self):
        self.metadata["version"] = "158.0"
        self.meta.write_text(json.dumps(self.metadata))
        with self.assertRaisesRegex(ValueError, "versions"):
            finalize(self.app, self.mar, self.meta)

    def test_reject_stale_noraneko_buildid(self):
        self.metadata["noraneko_buildid"] = "old"
        self.meta.write_text(json.dumps(self.metadata))
        with self.assertRaisesRegex(ValueError, "versions"):
            finalize(self.app, self.mar, self.meta)


if __name__ == "__main__":
    unittest.main()
