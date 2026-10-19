#!/usr/bin/env python3
"""Validate Chrome compatibility patches against the locked Runtime source."""

import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PATCH_NAMES = (
    "chrome-extension-native-messaging.patch",
    "chrome-extension-offscreen.patch",
    "chrome-side-panel.patch",
    "chrome-extension-messaging.patch",
    "chrome-extension-proxy.patch",
    "chrome-extension-diagnostics.patch",
    "chrome-extension-identity.patch",
    "chrome-extension-filesystem.patch",
    "chrome-extension-management.patch",
    "chrome-extension-url-alias.patch",
    "chrome-extension-webpage-messaging.patch",
    "chrome-extension-capture.patch",
    "chrome-extension-debugger.patch",
    "chrome-extension-desktop-capture.patch",
    "chrome-extension-desktop-source.patch",
    "chrome-extension-network.patch",
    "chrome-extension-response-headers.patch",
    "chrome-extension-power.patch",
    "chrome-extension-permission-messages.patch",
    "chrome-extension-tab-groups.patch",
    "chrome-extension-webrequest.patch",
    "chrome-extension-windows.patch",
    "chrome-extension-auxiliary.patch",
    "chrome-extension-auxiliary-content.patch",
)
NATIVE_ONLY_PATCH_NAMES = (
    "chrome-extension-debugger-native.patch",
    "chrome-extension-media-native.patch",
    "chrome-extension-network-native.patch",
    "chrome-extension-workers.patch",
    "chrome-extension-proxy-native.patch",
    "chrome-extension-power-native.patch",
    "chrome-extension-proxy-scopes-native.patch",
    "chrome-extension-fonts-native.patch",
    "chrome-extension-speech-native.patch",
    "chrome-extension-notifications-native.patch",
)


def run(arguments, cwd):
    result = subprocess.run(arguments, cwd=cwd, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace"))
    return result.stdout


def artifact_path(source):
    if source.startswith("browser/modules/"):
        return source
    if source.startswith("toolkit/locales/en-US/") and source.endswith(".ftl"):
        return "localization/en-US/" + source[len("toolkit/locales/en-US/"):]
    if source.startswith("toolkit/components/alerts/"):
        return "chrome/toolkit/content/global/alerts/" + source.rsplit("/", 1)[1]
    if source.startswith("browser/actors/"):
        return source
    if source.startswith("devtools/"):
        return "browser/chrome/devtools/modules/" + source
    toolkit = "toolkit/components/extensions/"
    browser = "browser/components/extensions/"
    addons = "toolkit/mozapps/extensions/content/"
    if source.startswith(addons):
        return "chrome/toolkit/content/mozapps/extensions/" + source[len(addons):]
    if source == "browser/base/content/webext-panels.js":
        return "browser/chrome/browser/content/browser/webext-panels.js"
    if source == "browser/components/sidebar/browser-sidebar.js":
        return "browser/chrome/browser/content/browser/sidebar/browser-sidebar.js"
    if source.startswith(toolkit):
        relative = source[len(toolkit):]
        if "/" not in relative and relative.endswith(".sys.mjs"):
            return "modules/" + relative
        if relative.startswith(("parent/", "child/", "schemas/")) or relative == "ext-toolkit.json":
            return "chrome/toolkit/content/extensions/" + relative
    if source.startswith(browser):
        return "browser/chrome/browser/content/browser/" + source[len(browser):]
    return None


def patch_files(patch):
    files = re.findall(r"^diff --git a/(\S+) b/\1$", patch.read_text(), re.MULTILINE)
    if not files or len(files) != len(set(files)):
        raise RuntimeError(f"Invalid or empty patch file list: {patch}")
    return files


def patch_changes(patch):
    changes = {}
    current = None
    for line in patch.read_text().splitlines():
        if line.startswith("diff --git "):
            current = line.split(" b/", 1)[1]
            changes[current] = []
        elif current and line[:1] in (" ", "+", "-") and not line.startswith(("--- ", "+++ ")):
            changes[current].append(line)
    return changes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-source", type=Path, default=ROOT.parent / "Floorp-Runtime")
    parser.add_argument("--all-source", action="store_true", help="Also compose every maintained common source patch")
    arguments = parser.parse_args()
    revision = json.loads((ROOT / "floorp-runtime.lock.json").read_text())["source"]["commit"]
    source_patches = [ROOT / ".github/patches/floorp-runtime/common" / name
                      for name in sorted((*PATCH_NAMES, *NATIVE_ONLY_PATCH_NAMES))]
    if arguments.all_source:
        source_patches = sorted((ROOT / ".github/patches/floorp-runtime/common").glob("*.patch"))
    missing = [str(patch) for patch in source_patches if not patch.is_file()]
    if missing:
        raise RuntimeError("Missing Runtime patches: " + ", ".join(missing))
    with tempfile.TemporaryDirectory(prefix="floorp-chrome-patches-") as temporary:
        fixture = Path(temporary)
        run(["git", "init", "--quiet"], fixture)
        for patch in source_patches:
            for source in patch_files(patch):
                target = fixture / source
                if target.exists():
                    continue
                original = subprocess.run(
                    ["git", "show", f"{revision}:{source}"],
                    cwd=arguments.runtime_source,
                    capture_output=True,
                )
                if original.returncode == 0:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(original.stdout)
            run(["git", "apply", "--check", str(patch)], fixture)
            run(["git", "apply", str(patch)], fixture)
            if patch.name in PATCH_NAMES:
                development = ROOT / "tools/patches" / patch.name
                if not development.is_file():
                    raise RuntimeError(f"Missing development patch: {development}")
                expected = {
                    artifact_path(source)
                    for source in patch_files(patch)
                    if artifact_path(source) is not None
                    and not source.endswith("jar.mn")
                    and "/test/" not in source
                }
                actual = set(patch_files(development))
                if actual != expected:
                    raise RuntimeError(f"Development paths differ for {patch.name}: {actual ^ expected}")
                source_changes = patch_changes(patch)
                development_changes = patch_changes(development)
                for source in patch_files(patch):
                    mapped = artifact_path(source)
                    if mapped in expected and source_changes[source] != development_changes[mapped]:
                        raise RuntimeError(f"Source and development code differ for {source}")
            for source in patch_files(patch):
                target = fixture / source
                if target.suffix == ".json":
                    json.loads(target.read_text())
                elif target.suffix in (".js", ".mjs") and not re.search(r"^#(?:if|ifdef|ifndef|include|filter)\b", target.read_text(), re.MULTILINE):
                    run(["node", "--check", str(target)], fixture)
            print(f"PASS {patch.name} ({len(patch_files(patch))} files)")
    print(f"PASS composed patches against locked Runtime {revision}")


if __name__ == "__main__":
    main()
