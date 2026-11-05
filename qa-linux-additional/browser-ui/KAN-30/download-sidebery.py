"""Fetch the public signed fixture; product installation still verifies its signature."""
import hashlib
import json
import subprocess
from pathlib import Path

root = Path(__file__).parent
metadata = json.loads((root / "sidebery-package.json").read_text())
target = root / "sidebery.xpi"
subprocess.run(["curl", "-fLsS", metadata["url"], "-o", str(target)], check=True)
assert target.stat().st_size == metadata["bytes"]
assert "sha256:" + hashlib.sha256(target.read_bytes()).hexdigest() == metadata["hash"]
print("Signed public fixture downloaded; hash and size match. No profile changes.")
