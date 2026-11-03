#!/usr/bin/env python3
"""Freeze relevant member references from signed corpus packages; no execution claim."""
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "_dist/chrome-extension-corpus"
OUTPUT = Path(__file__).with_name("auxiliary-api-inventory.json")
SELECTED = {"Windscribe", "1Password", "Bitwarden", "Checker Plus for Gmail", "Momentum", "Tampermonkey", "Automa", "UI.Vision"}
MEMBERS = (
    "storage.session.setAccessLevel", "notifications.update", "notifications.getAll",
    "notifications.onButtonClicked", "notifications.onClosed", "notifications.getPermissionLevel",
    "notifications.onPermissionLevelChanged", "action.openPopup", "action.getUserSettings",
    "action.onUserSettingsChanged", "declarativeContent.PageStateMatcher",
    "declarativeContent.RequestContentScript", "declarativeContent.onPageChanged",
)
PATTERNS = {member: re.compile(r"\b" + r"\s*(?:\?\.|\.)\s*".join(map(re.escape, member.split(".")))) for member in MEMBERS}
lock = json.loads((CORPUS / "corpus-lock.json").read_text())
result = {
    "schemaVersion": 1,
    "referenceChromeVersion": lock["referenceChromeVersion"],
    "runtimeSourceCommit": json.loads((ROOT / "floorp-runtime.lock.json").read_text())["source"]["commit"],
    "corpusFrozenAtUTC": lock["frozenAtUTC"],
    "scope": "Frozen signed-package member references selected for auxiliary API work. References include feature detection and are not primary-operation passes.",
    "extensions": [],
}
for extension in lock["extensions"]:
    if extension["name"] not in SELECTED or extension["status"] != "frozen":
        continue
    package = CORPUS / "packages" / (extension["id"] + ".crx")
    digest = hashlib.sha256(package.read_bytes()).hexdigest()
    if digest != extension["packageSha256"]:
        raise RuntimeError(f"Corpus package hash changed: {extension['id']}")
    entry = {key: extension[key] for key in ["id", "name", "version", "packageSha256"]}
    entry["references"] = []
    with zipfile.ZipFile(package) as archive:
        for source in archive.namelist():
            if not source.endswith((".js", ".mjs")):
                continue
            data = archive.read(source)
            text = data.decode("utf8", errors="replace")
            for member, pattern in PATTERNS.items():
                match = pattern.search(text)
                if match:
                    entry["references"].append({"member": member, "path": source,
                        "sourceSha256": hashlib.sha256(data).hexdigest(),
                        "firstCharacterOffset": match.start(),
                        "context": text[max(0, match.start()-50):match.end()+90]})
    result["extensions"].append(entry)
OUTPUT.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n")
print(f"Wrote {len(result['extensions'])} frozen package inventories to {OUTPUT}")
