"""Run with python3 -m unittest discover -s static/gecko/pref -p test_override.py.

Requires Bash, sed and Node.js; evaluates generated preferences, not Gecko.
"""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent


class OverrideTests(unittest.TestCase):
    def run_fixture(self, ini, initial=""):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copy2(ROOT / "override.sh", root / "override.sh")
            (root / "override.ini").write_text(ini, encoding="utf-8")
            output = root / "firefox.js"
            output.write_text(initial, encoding="utf-8")
            outputs = []
            values = []
            for _ in range(3):
                subprocess.run(
                    ["bash", str(root / "override.sh"), str(output)],
                    check=True, capture_output=True, text=True,
                )
                outputs.append(output.read_text(encoding="utf-8"))
                result = subprocess.run(
                    ["node", "-e", """
const vm = require('node:vm');
const fs = require('node:fs');
const values = {};
const record = (name, value) => { values[name] = value; };
vm.runInNewContext(fs.readFileSync(process.argv[1], 'utf8'), {
  pref: record, lockPref: record,
});
console.log(JSON.stringify(values));
""", str(output)],
                    check=True, capture_output=True, text=True,
                )
                values.append(json.loads(result.stdout))
            self.assertEqual(outputs[0], outputs[1])
            self.assertEqual(outputs[1], outputs[2])
            return values[0]

    def test_repository_json_defaults(self):
        values = self.run_fixture((ROOT / "override.ini").read_text())
        self.assertEqual(json.loads(values["floorp.splitView.config"]), {
            "layout": "horizontal", "maxPanes": 4,
        })
        self.assertEqual(json.loads(values["floorp.splitView.paneSizes"]), {
            "flexRatios": [0.5, 0.5], "gridColRatio": 0.5, "gridRowRatio": 0.5,
        })
        self.assertEqual(json.loads(values["floorp.memory.idleReclaim"]), {
            "enabled": False, "idleThresholdSec": 60, "pollIntervalSec": 60,
            "minIntervalSec": 300, "minResidentMB": 400,
            "reclaimOnGhostWindows": True,
        })

    def test_existing_preferences_and_replacement_metacharacters(self):
        ini = r'''test.json = "{\"key\":\"a&b|c\"}"
@test.locked = "{\"key\":1}"
test.path = "C:\\Temp\\file"
test.quote = "say "hello" today"
test.empty = ""
test.bool = true
test.number = 42'''
        expected = {
            "test.json": '{"key":"a&b|c"}',
            "test.locked": '{"key":1}',
            "test.path": r"C:\Temp\file",
            "test.quote": 'say "hello" today',
            "test.empty": "", "test.bool": True, "test.number": 42,
        }
        self.assertEqual(self.run_fixture(ini), expected)
        initial = '\n'.join(
            f'{"pref" if name == "test.locked" else "lockPref"}'
            f'("{name}", "old");' for name in expected
        ) + '\n'
        self.assertEqual(self.run_fixture(ini, initial), expected)


if __name__ == "__main__":
    unittest.main()
