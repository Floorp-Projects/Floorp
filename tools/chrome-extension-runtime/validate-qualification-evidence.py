#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Validate the frozen selection and exact historical/native evidence boundaries.

A candidate manifest is reproducible build provenance checked against an immutable
package and all installed payload files, not a vendor signature or attestation.
Passing this validator does not certify a release or replace actual primary tests.
"""
import argparse
import ast
from collections import Counter
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import struct
import sys
from urllib.parse import urlsplit
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
HEX32 = re.compile(r"[0-9a-f]{32}\Z")
HEX64 = re.compile(r"[0-9a-f]{64}\Z")
NATIVE_SCOPE = "exact-native-candidate"
HISTORICAL_SCOPE = "historical-prebuilt157-not-native158"
# Explicit source-reviewed adapters only. Unknown suite/schema combinations are
# rejected until their owner supplies a genuine primary and observable contract.
SUITES = {
    "tools/chrome-extension-runtime/test-corpus-content-floorp.mjs": {
        "stage": "actual-package-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "content",
        "ids": {"eimadpbcbfnmbkopoojfekhnkhdbieeh", "clngdbkpkpeebahjckkjfobafhncgmne", "jinjaccalgkegednnccohejagnlnfdag", "dhdgffkkebhmkfjojejmpbldmpobfkfo"}},
    "tools/chrome-extension-runtime/test-corpus-devtools-floorp.mjs": {
        "stage": "actual-package-devtools-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "devtools",
        "ids": {"fmkadmapgofadopljbjfkapdkoienihi", "lmhkpmbekcpmknklioeibfkpmmfibljd", "nhdogjmejiglipccpnnnanhbledajbpd"}},
    "tools/chrome-extension-runtime/test-corpus-media-content-floorp.mjs": {
        "stage": "actual-package-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "media",
        "ids": {"gebbhagfogifgggkldgodflihgfeippi", "ponfpcnoihfmfllpaingbgckeeldkhle", "mnjggcdmjocbbbhaepdhchncahnbgone"}},
    "tools/chrome-extension-runtime/test-corpus-sidepanel-floorp.mjs": {
        "stage": "actual-sidepanel-primary-operation", "result": "result", "success": "passed", "kind": "sidepanel",
        "ids": {"difoiogjjojoaoomphldepapgpbgkhkb", "ofpnmcalabcbjgholdjcjblkibolbppb", "camppjleccjaphfdbohjdohecfnoikec", "mhnlakgilnojmhinhkckjpncpbhabphi", "cebmnlammjhancocbbnfcglifgdpfejc", "gldebcpkoojijledacjeboaehblhfbjg", "befflofjcniongenjmbkgkoljhgliihe"}},
    "tools/chrome-extension-runtime/test-corpus-network-floorp.mjs": {
        "stage": "actual-package-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "network",
        "ids": {"pfnededegaaopdmhkdmcofjmoldfiped", "gcknhkkoolaabfmlnjonogaaifnjlfnp", "djflhoibgkdhkhhcedjiklpkjnoahfmg", "ddkjiahejlhfcafbddmgiahcphecmpfh"}},
    "tools/chrome-extension-runtime/test-corpus-automation-floorp.mjs": {
        "stage": "frozen-automation-primary-operation", "result": "result", "success": "pass", "kind": "automation",
        "ids": {"gcbalfbdmfieckjlnblleoemohcganoc", "infppggnoaenmfagbfknfkancpbljcca", "jnhgnonknehpejjnehehllkliplmbmhn"}},
    "tools/chrome-extension-runtime/test-corpus-webscraper-floorp.mjs": {
        "stage": "frozen-webscraper-primary-operation", "result": "result", "success": "pass", "kind": "webscraper",
        "ids": {"jnhgnonknehpejjnehehllkliplmbmhn"}},
    "tools/chrome-extension-runtime/test-corpus-bitwarden-floorp.mjs": {
        "stage": "actual-Bitwarden-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "bitwarden",
        "ids": {"nngceckbapebfimnlniiiahkandclblb"}},
    "browser-features/modules/modules/chrome-web-store/test/polyfills/capture/CaptureCorpusNative.test.mts": {
        "stage": "actual-package-primary-operation", "result": "primaryOperationResult", "success": "pass", "kind": "capture",
        "ids": {"nlipoenfbbikpbjkfpfillcgkoblgpmj", "fdpohaocaechififmbbbbbknoalclacl"}},
    "tools/chrome-extension-runtime/test-corpus-languagetool-floorp.mjs": {
        "stage": "actual-package-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "languagetool",
        "ids": {"oldceeleldhonbafppcapldpdifcinji"}},
    "tools/chrome-extension-runtime/test-corpus-browser-tools-floorp.mjs": {
        "stage": "actual-package-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "browsertools",
        "ids": {"hlkenndednhfkekhgcdicdfddnkalmdm", "chphlpgkkbolifaimnlloiipkdnihall", "edacconmaakjimmfgnblocblbcdcpbko", "laookkfknpbbblfpciffpaejjkokdgca"}},
    "tools/chrome-extension-runtime/test-corpus-management-floorp.mjs": {
        "stage": "actual-package-primary-operation", "result": "primaryOperationResult", "success": "passed", "kind": "management",
        "ids": {"jjmflmamggggndanpgfnpelongoepncg"}},
}


def require(condition, description):
    if not condition:
        raise ValueError(description)


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def evidence_file(root, path, digest):
    require(isinstance(path, str) and path, "Missing evidence path")
    relative = Path(path)
    require(not relative.is_absolute() and ".." not in relative.parts,
            "Evidence path must stay relative to the checkout")
    absolute = (root / relative).resolve(strict=True)
    require(absolute.is_relative_to(root.resolve()) and absolute.is_file(),
            "Evidence file escapes the checkout or is not a regular file")
    require(isinstance(digest, str) and HEX64.fullmatch(digest), "Missing evidence digest")
    require(sha(absolute) == digest, "Evidence digest mismatch: " + path)
    return absolute


def original_inventory(root, selected):
    """Independently calculate the signed archive's complete JS inventory.

    The existing converter verifies the signature during each browser install.
    This comparison authenticates the recorded JS names/bytes to that frozen
    archive's digest; it does not repeat signature-chain verification in Python.
    """
    path = root / "_dist/chrome-extension-corpus/packages" / (selected["id"] + ".crx")
    require(path.stat().st_size <= 100 * 1024 ** 2, "Frozen CRX exceeds size limit")
    require(sha(path) == selected["packageSha256"], "Frozen CRX digest mismatch")
    data = path.read_bytes()
    require(data[:4] == b"Cr24" and len(data) >= 12, "Invalid frozen CRX header")
    version = struct.unpack_from("<I", data, 4)[0]
    if version == 3:
        offset = 12 + struct.unpack_from("<I", data, 8)[0]
    elif version == 2 and len(data) >= 16:
        public_key, signature = struct.unpack_from("<II", data, 8)
        offset = 16 + public_key + signature
    else:
        raise ValueError("Unsupported frozen CRX version")
    require(12 <= offset < len(data), "Invalid frozen CRX ZIP offset")
    records, seen, expanded, manifest = {}, set(), 0, None
    with zipfile.ZipFile(io.BytesIO(data[offset:])) as archive:
        entries = archive.infolist()
        require(len(entries) <= 10000, "Frozen archive entry limit")
        for entry in entries:
            name = entry.filename
            require(name and "\\" not in name and "\0" not in name and
                    not name.startswith("/") and not re.match(r"^[A-Za-z]:", name) and
                    not any(part in (".", "..") for part in name.split("/")) and name not in seen,
                    "Unsafe or duplicate frozen archive path")
            seen.add(name)
            expanded += entry.file_size
            require(entry.file_size <= 50 * 1024 ** 2 and expanded <= 500 * 1024 ** 2,
                    "Frozen archive expansion limit")
            if name == "manifest.json":
                manifest = json.loads(archive.read(entry))
            elif re.search(r"\.(?:js|mjs|cjs)$", name, re.I):
                records[name] = hashlib.sha256(archive.read(entry)).hexdigest()
    require(isinstance(manifest, dict), "Frozen manifest missing")
    worker = manifest.get("background", {}).get("service_worker")
    require(worker is None or isinstance(worker, str) and bool(worker), "Invalid original service worker")
    # JavaScript .sort() compares UTF-16 code units; use the same ordering.
    names = sorted(records, key=lambda name: name.encode("utf-16-be", "surrogatepass"))
    serialized = "\n".join(name + ":" + records[name] for name in names).encode()
    return {"unchangedScriptCount": len(names),
            "unchangedScriptsSha256": hashlib.sha256(serialized).hexdigest(),
            "originalServiceWorker": worker}


def report_item(report, extension_id):
    candidates = []
    if isinstance(report.get("extensions"), list):
        candidates.extend(item for item in report["extensions"]
                          if isinstance(item, dict) and item.get("id") == extension_id)
    if isinstance(report.get("extension"), dict) and report["extension"].get("id") == extension_id:
        candidates.append(report["extension"])
    if report.get("id") == extension_id:
        candidates.append(report)
    require(len(candidates) == 1, "Actual report must contain exactly one matching extension item")
    return candidates[0]


def check_background(native, source, background, expected, identity, runtime, run_id):
    require(isinstance(native, dict) and isinstance(source, dict) and isinstance(background, dict),
            "Missing native/background/source evidence")
    for key in ("strictNative", "candidateIdentityVerified", "nativeBackgroundVerified", "workerWebIDLCompiled"):
        require(native.get(key) is True, "Missing observed native gate: " + key)
    require(native.get("runId") == run_id and HEX32.fullmatch(run_id),
            "Extension evidence belongs to a different or unrecorded browser run")
    require(native.get("buildScope") == "strict-native-candidate", "Native helper scope mismatch")
    # The install helper deliberately never claims the primary operation.
    require(native.get("nativeQualified") is False, "Install evidence cannot qualify a primary")
    require(native.get("candidateIdentity") == identity, "Native extension identity differs from authenticated wrapper")
    require(native.get("version") == runtime["version"] and native.get("buildId") == runtime["buildId"],
            "Native extension runtime differs from authenticated process")
    require(background.get("nativeWorkerCompiled") is True and background.get("serviceWorkerFallback") is False,
            "Native converter is unavailable or selected a fallback")
    require(native.get("background") == background, "Conversion/native background evidence differs")
    for key in ("unchangedScriptCount", "unchangedScriptsSha256", "originalServiceWorker"):
        require(source.get(key) == expected[key], "Original signed script inventory mismatch: " + key)
    require(native.get("originalServiceWorker") == expected["originalServiceWorker"], "Native original worker mismatch")
    require(source.get("convertedServiceWorker") == native.get("convertedServiceWorker"),
            "Converted worker evidence differs")
    worker = expected["originalServiceWorker"]
    if worker or source.get("convertedServiceWorker") or background.get("selected") == "service_worker":
        require(worker and source.get("convertedServiceWorker") == worker and
                background.get("selected") == "service_worker" and
                native.get("backgroundViewType") == "background_worker" and
                native.get("nativeWorkerVerified") is True,
                "Original native service worker was not actually observed")
    else:
        require(source.get("convertedServiceWorker") is None, "Unexpected worker in a document extension")
    # WebExtensionPolicy.backgroundServiceWorkerEnabled may be FALSE by design.



def trusted_action(action):
    return isinstance(action, dict) and action.get("focused") is True and any(
        event.get("type") == "click" and event.get("trusted") is True
        for event in action.get("nativeEvents", []) if isinstance(event, dict))


def decode_capture_png(data):
    """Bounded decode of the genuine fixture exports: noninterlaced8-bit RGB/A.

    Validate every chunk CRC and complete compressed scanline stream, then
    reconstruct the native header sample at20,20. Unknown formats fail closed.
    """
    require(24 <= len(data) <= 32 * 1024 ** 2 and data[:8] == b"\x89PNG\r\n\x1a\n",
            "Invalid or oversized saved PNG")
    offset, compressed, dimensions, ended, idat_ended = 8, bytearray(), None, False, False
    while offset < len(data):
        require(len(data) - offset >= 12, "Truncated saved PNG chunk")
        size = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        require(all(65 <= byte <= 90 or 97 <= byte <= 122 for byte in kind), "Invalid saved PNG chunk type")
        end = offset + 12 + size
        require(end <= len(data), "Truncated saved PNG chunk payload")
        payload = data[offset + 8:offset + 8 + size]
        require(zlib.crc32(kind + payload) & 0xffffffff == struct.unpack_from(">I", data, end - 4)[0],
                "Saved PNG chunk CRC mismatch")
        if dimensions is None:
            require(kind == b"IHDR" and size == 13, "Saved PNG must start with exact IHDR")
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            require(20 < width <= 16384 and 20 < height <= 16384 and width * height <= 16 * 1024 ** 2 and
                    depth == 8 and color in (2, 6) and compression == filtering == interlace == 0,
                    "Unsupported or oversized native capture PNG format")
            dimensions = width, height, 3 if color == 2 else 4
        elif kind == b"IDAT":
            require(not idat_ended, "Saved PNG has nonconsecutive image data")
            compressed.extend(payload)
        elif kind == b"IEND":
            require(size == 0 and end == len(data) and compressed, "Invalid saved PNG end/image data")
            ended = True
        else:
            require(kind != b"IHDR" and (kind[0] & 32 or kind == b"PLTE"), "Unknown critical saved PNG chunk")
            if kind == b"PLTE":
                require(not compressed and 0 < size <= 768 and size % 3 == 0, "Invalid saved PNG palette")
            if compressed:
                idat_ended = True
        offset = end
    require(ended and dimensions is not None, "Incomplete saved PNG")
    width, height, channels = dimensions
    stride = width * channels
    expected = height * (stride + 1)
    try:
        decoder = zlib.decompressobj()
        raw = decoder.decompress(compressed, expected + 1)
    except zlib.error as error:
        raise ValueError("Invalid saved PNG compressed image data") from error
    require(len(raw) == expected and decoder.eof and not decoder.unconsumed_tail and not decoder.unused_data,
            "Saved PNG image data has wrong dimensions or incomplete/excess compressed bytes")
    require(all(raw[row * (stride + 1)] <= 4 for row in range(height)), "Invalid saved PNG scanline filter")
    previous = bytearray(stride)
    for row in range(21):
        start = row * (stride + 1)
        filtering = raw[start]
        current = bytearray(raw[start + 1:start + 1 + stride])
        for column in range(stride):
            left = current[column - channels] if column >= channels else 0
            above = previous[column]
            upper_left = previous[column - channels] if column >= channels else 0
            if filtering == 1:
                predictor = left
            elif filtering == 2:
                predictor = above
            elif filtering == 3:
                predictor = (left + above) // 2
            elif filtering == 4:
                prediction = left + above - upper_left
                distances = abs(prediction - left), abs(prediction - above), abs(prediction - upper_left)
                predictor = left if distances[0] <= min(distances[1:]) else above if distances[1] <= distances[2] else upper_left
            else:
                predictor = 0
            current[column] = (current[column] + predictor) & 255
        previous = current
    pixel = list(previous[20 * channels:21 * channels])
    return width, height, pixel if channels == 4 else pixel + [255]


def check_saved_capture(root, report, item, *, native):
    media = item.get("media", {})
    require(media.get("pixel") == [41, 126, 216, 255] and
            isinstance(media.get("width"), int) and media["width"] > 500 and
            isinstance(media.get("height"), int) and media["height"] > 0 and
            isinstance(media.get("bytes"), int) and 10000 < media["bytes"] <= 32 * 1024 ** 2,
            "Actual decoded authored-page PNG pixels and size missing")
    if item["id"] == "fdpohaocaechififmbbbbbknoalclacl":
        require(media["height"] == 2590, "Actual full-page capture must stitch the complete2590px fixture")
    path = Path(media.get("path", "")).resolve(strict=True)
    require(path.is_relative_to((root / "_dist/chrome-runtime-tests").resolve()) and path.suffix == ".png",
            "Saved capture must be a retained test artifact")
    require(path.parent == (Path(report.get("output", "")) / "downloads").resolve(strict=True),
            "Saved capture belongs to a different report output")
    evidence_file(root, str(path.relative_to(root)), media.get("sha256"))
    require(path.stat().st_size == media["bytes"], "Saved native PNG byte length differs")
    width, height, pixel = decode_capture_png(path.read_bytes())
    require((width, height, pixel) == (media["width"], media["height"], media["pixel"]),
            "Saved native PNG bytes independently decode different dimensions or authored pixels")
    if native:
        image = item.get("image", {})
        saved = item.get("savedImage", {})
        require(item.get("cleanupCompleted") is True and item.get("nativeQualified") is True and
                trusted_action(item.get("nativeAction")) and
                image.get("pixel") == saved.get("pixel") == media["pixel"] and
                image.get("width") == saved.get("width") == media["width"] and
                image.get("height") == saved.get("height") == media["height"],
                "Actual native action, preview, saved-file decode and cleanup did not complete")
        if item["id"] == "nlipoenfbbikpbjkfpfillcgkoblgpmj":
            require(image.get("sha256") == media.get("sha256") and
                    item.get("canvas", {}).get("width", 0) > 500,
                    "Saved visible screenshot differs from the genuine decoded preview")
        source = urlsplit(item.get("source", {}).get("url", ""))
        require(source.scheme == "http" and source.hostname == "127.0.0.1" and
                source.path == "/corpus-capture.html", "Actual capture did not target the authored fixture")
    else:
        require(isinstance(item.get("steps"), list) and any("Download image UI" in step for step in item["steps"]),
                "Actual Chrome capture Download control was not completed")


def check_browser_tool_observables(report, item, *, native):
    snapshots = {value.get("label"): value for value in item.get("snapshots", []) if isinstance(value, dict)}
    checks = item["checks"]
    if item["id"] == "hlkenndednhfkekhgcdicdfddnkalmdm":
        expected = ("Actual product UI created, edited, and deleted the cookie; authored page independently observed each effect"
                    if native else "Actual native product UI created, edited, and deleted a cookie; independent authored page observed all three effects")
        require(expected in checks and (not native or trusted_action(item.get("action"))) and
                any("floorp_authored_cookie" in value.get("text", "") for label, value in snapshots.items() if "created" in label) and
                any("floorp_authored_cookie" in value.get("text", "") for label, value in snapshots.items() if "edited" in label),
                "Genuine Cookie Editor create/edit/delete and independent page observations missing")
    elif item["id"] == "laookkfknpbbblfpciffpaejjkokdgca":
        for label in ("guest-dashboard", "after-native-reload"):
            snapshot = snapshots.get(label, {})
            require("Authored User" in snapshot.get("text", "") and
                    "How would you like to log in" not in snapshot["text"] and
                    not any(control.get("id") == "name" for control in snapshot.get("controls", [])),
                    "Actual Momentum guest setting did not persist in its real reloaded dashboard")
        if native:
            windows = item.get("reloadWindows", {})
            require(isinstance(windows.get("before"), int) and isinstance(windows.get("after"), int) and
                    windows["before"] != windows["after"], "Momentum persistence requires a distinct actual reloaded document")
        require(("A distinct native document after reload retained the authored name" if native else
                 "Actual dashboard UI persisted authored setting across native reload") in checks,
                "Actual Momentum persistence check missing")
    else:
        session = item["id"] == "edacconmaakjimmfgnblocblbcdcpbko"
        saved = snapshots.get("actual-saved-collection" if session else "saved-session", {})
        require(all(title in saved.get("text", "") for title in ("Authored saved session /first", "Authored saved session /second")),
                "Real collection did not contain both authored tabs")
        if session:
            require("Authored Session" in saved["text"] and
                    ("Actual Save UI created named collection with both authored tabs; collection survived source-tab closure and distinct-document reload; actual Open restored both exact URLs"
                     if native else "Saved both authored tabs through actual Save UI, closed source tabs, reloaded the stored collection, then Open restored both exact URLs") in checks,
                    "Actual Session Buddy save/reload/restore side effects missing")
        else:
            require(("Actual Store all open windows UI saved and closed both authored tabs" if native else
                     "Real OneTab Store all open windows UI saved and closed both authored tabs") in checks and
                    "Actual Restore all UI recreated both authored tabs" in checks,
                    "Actual OneTab save/close/restore side effects missing")
        if native:
            pages = report.get("fixture", {}).get("pages", [])
            require(len(pages) == 2 and all(urlsplit(page.get("url", "")).hostname == "127.0.0.1" and
                        HEX64.fullmatch(page.get("sha256", "")) for page in pages),
                    "Exact authored session URLs and fixture bytes missing")


def check_primary_observables(root, row, adapter, report, item, *, native=True):
    kind = adapter["kind"]
    if kind == "bitwarden" and not native:
        require(report.get("passed") is True and report.get("initialFieldsEmpty") is True and
                report.get("fillMatches", {}).get("username") is True and
                report.get("fillMatches", {}).get("password") is True and not report.get("error") and
                report.get("authoredSourceSha256") == sha(root / "tools/chrome-extension-runtime/fixtures/corpus-bitwarden-login.html"),
                "Actual Chrome Bitwarden authored fill was not completed")
        return
    require(item.get(adapter["result"]) == adapter["success"], "Exact primary adapter result did not pass")
    require(not item.get("error") and not item.get("cleanupError"), "Primary operation or cleanup failed")
    if native and "nativeQualified" in item:
        require(item["nativeQualified"] is True, "Suite explicitly rejected final native qualification")
    if kind == "bitwarden":
        reference_path = evidence_file(root, row["chrome"].get("reportPath"), row["chrome"].get("reportSha256"))
        reference = json.loads(reference_path.read_text())
        reference_item = report_item(reference, item["id"])
        url = report.get("authoredURL")
        fixture_sha = sha(root / "tools/chrome-extension-runtime/fixtures/corpus-bitwarden-login.html")
        require(isinstance(url, str) and urlsplit(url).scheme == "https" and
                urlsplit(url).hostname == "corpus.floorp.test" and urlsplit(url).path == "/login" and
                url == reference.get("authoredURL") and
                report.get("authorFixtureProbe", {}).get("secure") is True and
                report.get("authorFixtureProbe", {}).get("url") == url,
                "Bitwarden requires the real secure exact authored URI")
        require(Path(report.get("referenceReport", "")).resolve() == reference_path and
                reference.get("passed") is True and
                all(reference_item.get(key) == item.get(key) for key in ("id", "version", "packageSha256")) and
                report.get("authoredSourceSha256") == reference.get("authoredSourceSha256") == fixture_sha,
                "Bitwarden must use the same frozen package and authored bytes as its genuine Chrome reference")
        require(report.get("passed") is True and report.get("primaryOperationResult") == "passed" and
                report.get("initialFieldsEmpty") is True and report.get("cleanupCompleted") is True and
                not report.get("cleanupErrors") and not report.get("error") and
                report.get("fillMatches", {}).get("username") is True and
                report.get("fillMatches", {}).get("password") is True,
                "Actual secure Bitwarden fill/cleanup was not completed")
        return
    if kind == "capture":
        check_saved_capture(root, report, item, native=native)
        return
    require(isinstance(item.get("checks"), list) and item["checks"] and
            all(isinstance(check, str) and check for check in item["checks"]),
            "Explicit primary operation checks missing")
    if kind == "browsertools":
        check_browser_tool_observables(report, item, native=native)
    elif kind == "management":
        require("Trusted click on unchanged Extensity UI disabled the authored extension and stopped its actual content script" in item["checks"] and
                "Second trusted UI click re-enabled the extension and restored its actual content script" in item["checks"],
                "Actual Extensity disable/re-enable and content-script lifecycle checks missing")
        if native:
            ordinary = report.get("fixture", {}).get("ordinaryFirefoxControl", {})
            require("setEnabled can only" in ordinary.get("error", "") and ordinary.get("unexpectedlyAllowed") is not True and
                    item.get("after") and "Authored lifecycle probe" in item.get("before", {}).get("text", ""),
                    "Actual management primary must preserve the ordinary Firefox permission boundary")
    elif kind == "languagetool":
        url = report.get("fixture", {}).get("url")
        require(isinstance(url, str) and urlsplit(url).scheme == "http" and
                urlsplit(url).hostname == "127.0.0.1", "LanguageTool authored fixture URL missing")
        for key, expected_url in (("transformed", url), ("afterNavigation", url + "second")):
            result = item.get(key, {})
            require(result.get("errors", {}).get("url") == result.get("corrected", {}).get("url") == expected_url and
                    result.get("errors", {}).get("installed") == "true" and
                    result.get("errors", {}).get("text") == "She go to school." and
                    result.get("corrected", {}).get("text") == "She goes to school." and
                    result.get("corrected", {}).get("ready") == "complete",
                    "Actual LanguageTool UI must correct the authored sentence in both real documents")
        removed = item.get("afterRemoval", {})
        require(removed.get("url") == url + "removed" and
                removed.get("text") == "She go to school." and not removed.get("installed") and
                removed.get("product") == [] and removed.get("ready") == "complete",
                "Actual LanguageTool uninstall negative control missing")
        if native:
            require(item.get("cleanupCompleted") is True and item.get("nativeQualified") is True and
                    any(request.get("status") == 200 and request.get("method") == "POST" and
                        re.fullmatch(r"https://(?:api(?:-fallback)?\.languagetool\.org|api\.languagetoolplus\.com)/v2/check", request.get("url", ""))
                        for request in item.get("nativeServiceRequests", [])),
                    "Actual native LanguageTool HTTPS check response/cleanup missing")
    elif kind == "sidepanel":
        sentinel = "Floorp authored compatibility passage 73195: Arctic foxes cross the quiet bridge."
        draft = "Floorp retained draft " + item["id"][:8] + " 84219"
        require((not native or item.get("nativeQualified") is True and
                 trusted_action(item.get("nativeAction")) and item.get("injectedPagePanel") is False) and
                item.get("readEvidence", {}).get("domContainsAuthoredText") is True and
                sentinel in item.get("afterReadUI", {}).get("text", "") and
                item.get("draftRetained") is True and draft in item.get("draftBefore", "") and
                item.get("draftAfter") == item.get("draftBefore"),
                "Actual native product UI must read the authored page and retain the draft across tabs")
    elif kind == "network":
        require(not native or item.get("nativeQualified") is True, "Network primary/cleanup checks did not complete")
        if item["id"] in {"pfnededegaaopdmhkdmcofjmoldfiped", "gcknhkkoolaabfmlnjonogaaifnjlfnp"}:
            restored = item.get("restored", {})
            disabled = restored.get("role") == "origin" or (
                item["id"] == "gcknhkkoolaabfmlnjonogaaifnjlfnp" and restored.get("failed") is True and
                item.get("restoredConfig", {}).get("value", {}).get("mode") == "system")
            require(item.get("routed", {}).get("role") == "proxy" and
                    item.get("bypassed", {}).get("role") == "origin" and disabled,
                    "Product UI must route real HTTP, bypass the authored origin and restore direct routing")
        elif item["id"] == "djflhoibgkdhkhhcedjiklpkjnoahfmg":
            require("Firefox/33" in item.get("echo", {}).get("headers", {}).get("user-agent", ""),
                    "Actual server must observe the selected product User-Agent")
        else:
            require(item.get("baselineAdLoaded") is True and item.get("blocked") is True and
                    ("Actual bundled static rules block authored script; No filtering UI allows it after native reload" in item["checks"] if native else
                     "Actual product static rules block controlled ad script; No filtering UI allows it after reload" in item["checks"]),
                    "Real bundled blocking and No filtering negative control must complete")
    elif kind in {"automation", "webscraper"}:
        if item["id"] == "jnhgnonknehpejjnehehllkliplmbmhn":
            rows = item.get("rows", [])
            url = item.get("sitemap", {}).get("startUrl", [])
            require(len(url) == 1 and len(rows) == 3 and
                    [entry[-2:] for entry in rows[1:]] == [["Apple", "3.25"], ["Pear", "4.50"]] and
                    all(len(entry) >= 3 and entry[1] == url[0] for entry in rows[1:]) and
                    (item.get("nativePanel", {}).get("label") == "Web Scraper" and item.get("input") if native else
                     item.get("nativePanel", {}).get("url", "").startswith("chrome-extension://" + item["id"] + "/devtools_web_scraper_panel.html")),
                    "Real native Web Scraper panel must extract exact authored product rows and source URL")
        else:
            vision = item["id"] == "gcbalfbdmfieckjlnblleoemohcganoc"
            expected = "Frozen package 42" if vision else "Automa native 73195"
            observed = item.get("observed", {})
            require(observed.get("input") == observed.get("operation") == expected and
                    observed.get("result") == "Submitted: " + expected and observed.get("clickCount") == "1",
                    "Real macro engine must type the authored input and perform exactly one submit click")
            if vision:
                require((not native or trusted_action(item.get("nativeAction"))) and item.get("macro") and
                        "Macro completed" in item.get("engineLog", "") and "[error]" not in item["engineLog"],
                        "UI.Vision native action and genuine macro completion log required")
            else:
                require(item.get("workflow", {}).get("name") == "Floorp authored macro" and
                        ("native mouse click on real Execute control" in item["checks"] if native else
                         isinstance(item.get("executeControl", {}).get("x"), (int, float)) and
                         isinstance(item.get("executeControl", {}).get("y"), (int, float))),
                        "Actual Automa workflow and native Execute control required")
    elif kind == "content":
        baseline, changed, navigated, removed = [item.get(key, {}) for key in
            ("baseline", "transformed", "afterNavigation", "afterRemoval")]
        require(all(isinstance(value, dict) and value.get("ready") == "complete"
                    for value in (baseline, changed, navigated)) and
                isinstance(removed, dict) and removed.get("ready") in ({"complete"} if native else {"complete", "interactive"}),
                "Missing real content document observations")
        require(removed.get("body") == baseline.get("body") and removed.get("probe") == baseline.get("probe") and
                not removed.get("markers"), "Actual uninstall did not restore the authored page")
        if item["id"] == "eimadpbcbfnmbkopoojfekhnkhdbieeh":
            require(all(value.get("markers") and value.get("body") != baseline.get("body")
                        for value in (changed, navigated)), "Actual DarkReader theme/navigation absent")
        else:
            require(all(value.get("probe") == "rgb(12, 34, 56)" for value in (changed, navigated)),
                    "Actual authored style effect/navigation absent")
            if item["id"] != "clngdbkpkpeebahjckkjfobafhncgmne":
                require(all(value.get("userScript") == "authored-primary-operation" and
                            value.get("frameUserScript") == "authored-primary-operation"
                            for value in (changed, navigated)), "Actual top/frame userscript/navigation absent")
    elif kind == "devtools":
        require(item.get("nativePanel"), "Actual registered native DevTools panel missing")
        if item["id"] == "fmkadmapgofadopljbjfkapdkoienihi":
            for key, expected in (("propsBefore", "0"), ("propsAfter", "1")):
                props = item.get(key, {}).get("props", {}) if native else item.get(key, {})
                inputs = props.get("inputs", [])
                require(len(inputs) >= 2 and inputs[0].get("value") == "count" and inputs[1].get("value") == expected,
                        "Actual native React component props absent")
            require(not native or item.get("appAfter", {}).get("reactCount") == "React count: 1", "Actual React app state did not update")
        elif item["id"] == "nhdogjmejiglipccpnnnanhbledajbpd":
            for key, expected in (("selectedComponent", "0"), ("updatedComponent", "1")):
                text = item.get(key, {}).get("text", "")
                require("FloorpVueCounterLabel" in text and re.search(r"\bcount\b\s*:?\s*" + expected + r"\b", text),
                        "Actual native Vue component props absent")
            require(not native or item.get("appAfter", {}).get("vueCount") == "Vue count: 1", "Actual Vue app state did not update")
        else:
            require(re.search(r"^count\(pin\):0$", item.get("stateBefore", {}).get("text", ""), re.M) and
                    re.search(r"^count\(pin\):1$", item.get("stateAfter", {}).get("text", ""), re.M) and
                    "AUTHORED_INCREMENT" in item.get("stateAfter", {}).get("text", "") and
                    (not native or item.get("appAfter", {}).get("reduxCount") == "Redux count: 1"), "Actual native Redux store/action absent")
        require(any("uninstall" in check.lower() for check in item["checks"]), "Native panel uninstall check missing")
    elif kind == "media":
        baseline, changed, navigated, removed = [item.get(key, {}) for key in
            ("baseline", "transformed", "afterNavigation", "afterRemoval")]
        require(all(isinstance(value, dict) and value.get("ready") == "complete"
                    for value in (baseline, changed, navigated, removed)), "Actual media document observations missing")
        if item["id"] == "mnjggcdmjocbbbhaepdhchncahnbgone":
            url = "https://www.youtube.com/watch?v=FloorpTest1"
            require(baseline.get("url") == changed.get("url") == url and
                    navigated.get("url") == removed.get("url") == url + "&second=1",
                    "SponsorBlock requires distinct real authored navigation and uninstall documents")
            for value in (changed, navigated):
                video, samples = value.get("video", {}), value.get("samples", [])
                require(video.get("paused") is False and video.get("currentTime", 0) >= 7 and
                        isinstance(samples, list) and len(samples) > 1 and
                        0 <= samples[-1].get("elapsed", -1) < 5 and
                        video["currentTime"] > samples[-1]["elapsed"] + 3,
                        "Actual SponsorBlock must seek before natural playback in both real documents")
            video = removed.get("video", {})
            require(video.get("paused") is False and 2.6 <= video.get("currentTime", 0) < 4.5 and
                    any(request.get("host") == "sponsor.ajay.app" and
                        urlsplit(request.get("path", "")).path == "/api/skipSegments/99296"
                        for request in report.get("nativeTransportRequests", [])),
                    "Actual SponsorBlock native TLS request and uninstall continuous-playback negative control missing")
        elif item["id"] == "gebbhagfogifgggkldgodflihgfeippi":
            require(all("123" in value.get("dislike", "") for value in (changed, navigated)) and
                    removed.get("dislike") == baseline.get("dislike"), "Actual dislike metadata/navigation/uninstall absent")
            require(not native or any(request.get("host") == "returnyoutubedislikeapi.com" and
                        request.get("path", "").startswith("/votes?videoId=FloorpTest1")
                        for request in report.get("nativeTransportRequests", [])), "Actual native metadata TLS request missing")
        else:
            require(all(abs(value.get("video", {}).get("playbackRate", 0) - 1.02) < 0.00001 and
                        value.get("bezel") == "1.02x" for value in (changed, navigated)) and
                    removed.get("video", {}).get("playbackRate") == 1 and "bezel" not in removed,
                    "Actual native playback control/navigation/uninstall absent")


SUITE_ACTORS = {
    "content": {"CorpusContentChild.sys.mjs", "CorpusContentUIChild.sys.mjs"},
    "devtools": {"CorpusDevtoolsChild.sys.mjs"},
    "media": {"CorpusMediaChild.sys.mjs"},
    "sidepanel": {"CorpusSidePanelChild.sys.mjs", "corpus-sidepanel-ui.sys.mjs"},
    "network": {"CorpusNetworkChild.sys.mjs"},
    "automation": {"CorpusAutomationChild.sys.mjs"},
    "webscraper": {"CorpusAutomationChild.sys.mjs"},
    "bitwarden": {"CorpusBitwardenChild.sys.mjs"},
    "capture": set(),
    "languagetool": {"CorpusLanguageToolChild.sys.mjs"},
    "browsertools": {"CorpusOperationsChild.sys.mjs", "corpus-sidepanel-ui.sys.mjs"},
    "management": {"CorpusOperationsChild.sys.mjs", "corpus-sidepanel-ui.sys.mjs"},
}


def reviewed_browser_globals(root):
    """Read the current reviewed generator's literal without executing Python."""
    tree = ast.parse((root / "tools/chrome-extension-runtime/run-browser-tests.py").read_text())
    literals = [node.args[0].value for node in ast.walk(tree)
                if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and
                isinstance(node.func.value, ast.Name) and node.func.value.id == "browser_globals" and
                node.func.attr == "write_text" and len(node.args) == 1 and
                isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str)]
    require(len(literals) == 1, "Generated browser globals need a reviewed literal contract")
    return literals[0].encode("utf-8")


def check_bundle_inputs(root, wrapper_path, index, module, suite_path, bundle):
    metafile = wrapper_path.parent / "modules" / f"test-{index}.meta.json"
    evidence_file(root, str(metafile.relative_to(root)), module.get("bundleMetafileSha256"))
    metadata = json.loads(metafile.read_text())
    require(isinstance(metadata.get("inputs"), dict) and metadata["inputs"], "Actual esbuild input inventory missing")
    expected = {}
    for name, details in metadata["inputs"].items():
        path = (Path(name) if Path(name).is_absolute() else root / name).resolve(strict=True)
        require(path not in expected and isinstance(details, dict) and type(details.get("bytes")) is int,
                "Duplicate or invalid esbuild input inventory")
        expected[path] = details["bytes"]
    inputs = module.get("bundleInputs")
    require(isinstance(inputs, list) and inputs, "Host bundle lacks complete reviewed input sources")
    actual = set()
    generated = (wrapper_path.parent / "modules/browser-globals.mjs").resolve(strict=True)
    for item in inputs:
        require(isinstance(item, dict) and isinstance(item.get("path"), str), "Invalid actual bundle input")
        path = Path(item["path"])
        require(path.is_absolute() and path == path.resolve(strict=True) and path.is_file() and path not in actual,
                "Actual bundle source must be a unique canonical file")
        actual.add(path)
        require(path in expected and type(item.get("bytes")) is int and
                item["bytes"] == expected[path] == path.stat().st_size and item.get("sha256") == sha(path),
                "Reviewed imported bundle source differs from the actual build input")
        if path == generated:
            require(path.read_bytes() == reviewed_browser_globals(root), "Generated browser globals differ from the reviewed runner")
        else:
            require(path.is_relative_to(root.resolve()), "Unreviewed source outside the candidate checkout")
    require(actual == set(expected) and suite_path in actual and generated in actual,
            "Host bundle sources must match every exact esbuild input")
    outputs = metadata.get("outputs", {})
    require(isinstance(outputs, dict) and len(outputs) == 1, "Unexpected esbuild output inventory")
    output_name, output = next(iter(outputs.items()))
    output_path = (Path(output_name) if Path(output_name).is_absolute() else root / output_name).resolve(strict=True)
    require(output_path == bundle.resolve(strict=True) and output.get("bytes") == bundle.stat().st_size,
            "Actual bundle and esbuild output inventory differ")


def check_actor_sources(root, wrapper_path, wrapper):
    expected = set()
    for module in wrapper.get("modules", []):
        source = module.get("path")
        if not isinstance(source, str):
            continue
        path = Path(source).resolve(strict=True)
        if path.is_relative_to(root.resolve()):
            adapter = SUITES.get(str(path.relative_to(root)))
            if adapter is not None:
                expected.update(SUITE_ACTORS[adapter["kind"]])
    actors = wrapper.get("actorModules")
    require(isinstance(actors, list), "Host copied actor source evidence missing")
    observed = set()
    for actor in actors:
        require(isinstance(actor, dict) and isinstance(actor.get("path"), str), "Invalid host actor source")
        source = Path(actor["path"])
        require(source.is_absolute() and source == source.resolve(strict=True) and source.is_file() and
                source.parent == root / "tools/chrome-extension-runtime" and source.name not in observed,
                "Private actor must use a unique reviewed source file")
        observed.add(source.name)
        copy = wrapper_path.parent / "modules/actors" / source.name
        loaded = wrapper_path.parent / "profile/chrome/runtime-actors" / source.name
        require(actor.get("copiedPath") == str(copy.resolve(strict=True)) and
                actor.get("loadedPath") == str(loaded.resolve()) and
                type(actor.get("bytes")) is int and actor["bytes"] == source.stat().st_size == copy.stat().st_size and
                actor.get("sha256") == sha(source) == sha(copy),
                "Loaded/durable actor copy differs from the reviewed original source")
    require(observed == expected, "Private actor inventory differs from the reviewed primary suites")


def check_suite_provenance(root, wrapper_path, wrapper, suite):
    """Bind the reviewed entry source and actual executed bundle to one host run."""
    require(isinstance(wrapper.get("runId"), str) and HEX32.fullmatch(wrapper["runId"]),
            "Host run lacks its exact browser-process runId")
    relative = Path(suite) if isinstance(suite, str) else Path("..")
    require(not relative.is_absolute() and ".." not in relative.parts and suite in SUITES,
            "Suite path has no reviewed primary adapter")
    suite_path = (root / relative).resolve(strict=True)
    require(suite_path.is_relative_to(root.resolve()) and suite_path.is_file(), "Unsafe suite source path")
    modules = wrapper.get("modules", [])
    require(isinstance(modules, list), "Host module evidence missing")
    matching = [(index, module) for index, module in enumerate(modules)
                if isinstance(module, dict) and isinstance(module.get("path"), str) and
                Path(module["path"]).resolve() == suite_path]
    require(len(matching) == 1, "Host run must identify exactly one reviewed primary suite")
    index, module = matching[0]
    require(module.get("done") is True and module.get("passed") is True,
            "Matching actual primary suite did not finish successfully")
    require(module.get("testSourceSha256") == sha(suite_path),
            "Reviewed primary source differs from the source recorded while bundling")
    bundle = wrapper_path.parent / "modules" / f"test-{index}.mjs"
    evidence_file(root, str(bundle.relative_to(root)), module.get("bundleSha256"))
    check_bundle_inputs(root, wrapper_path, index, module, suite_path, bundle)
    check_actor_sources(root, wrapper_path, wrapper)
    return wrapper["runId"]


def candidate_binding(root, evidence, selected, candidate, candidate_module, expected_manifest_sha):
    require(candidate is not None and candidate_module is not None and
            isinstance(expected_manifest_sha, str) and HEX64.fullmatch(expected_manifest_sha),
            "Candidate evidence requires authenticated package and browser inputs")
    binding = evidence.get("candidateBinding")
    require(isinstance(binding, dict), "Candidate row must reference its authenticated host run")
    wrapper_path = evidence_file(root, binding.get("runReportPath"), binding.get("runReportSha256"))
    wrapper = json.loads(wrapper_path.read_text())
    require(wrapper.get("nativeCandidateIdentityMatches") is True, "Host run did not authenticate running native identity")
    recorded = wrapper.get("candidate")
    require(isinstance(recorded, dict), "Authenticated host package evidence missing")
    for key in ("manifest", "packageSha256", "binarySha256", "applicationInventorySha256",
                "verifiedFileCount", "expectedVersion"):
        require(recorded.get(key) == candidate.get(key), "Host candidate payload changed: " + key)
    runtime = wrapper.get("runtime", {})
    candidate_module.verify_runtime_identity(candidate, runtime)
    require(evidence.get("runtime") == runtime, "Registry runtime differs from actual authenticated process")
    require(wrapper.get("passed") is True, "Candidate suite did not complete successfully")
    suite = binding.get("suitePath")
    run_id = check_suite_provenance(root, wrapper_path, wrapper, suite)
    report_path = evidence_file(root, evidence.get("reportPath"), evidence.get("reportSha256"))
    report = json.loads(report_path.read_text())
    adapter = SUITES.get(suite)
    require(adapter is not None and selected["id"] in adapter["ids"] and
            report.get("stage") == adapter["stage"] and report.get("stage") == evidence.get("reportStage"),
            "Suite/stage/frozenID primary adapter is unavailable or mismatched")
    item = report_item(report, selected["id"])
    manifest = candidate["manifest"]
    identity = {"manifestSha256": expected_manifest_sha, "packageSha256": candidate["packageSha256"],
                **{key: manifest[key] for key in
                   ("sourceCommit", "floorpCommit", "patches", "buildId", "platform")}}
    require(item.get("nativeEvidence", {}).get("runId") == run_id,
            "Primary report and successful suite came from different browser runs")
    return report, item, identity, runtime, adapter, run_id


def check_observation(root, evidence, *, check_artifacts):
    require(isinstance(evidence.get("nativeQualified"), bool), "Native qualification must be an explicit boolean")
    require(evidence.get("operationStatus") and evidence.get("buildScope"), "Missing operation scope")
    if evidence.get("evidenceKind") == "ui-inventory":
        require(evidence["operationStatus"] == "not-run", "Inventory is not a primary operation")
    if evidence.get("evidenceKind") == "actual-package-prerequisite-only":
        require(evidence.get("primaryOperationAttempted") is False and
                evidence["operationStatus"] != "primary-passed" and evidence.get("observedPrerequisiteStatus"),
                "A prerequisite gate cannot pass a primary")
    path = evidence.get("reportPath")
    if path:
        require(evidence.get("runtime"), "Evidence runtime missing")
        relative = Path(path)
        require(not relative.is_absolute() and ".." not in relative.parts, "Unsafe evidence path")
        require(isinstance(evidence.get("reportSha256"), str) and HEX64.fullmatch(evidence["reportSha256"]),
                "Missing report digest")
        if check_artifacts:
            actual = json.loads(evidence_file(root, path, evidence["reportSha256"]).read_text())
            require(evidence.get("reportStage") == actual.get("stage"), "Recorded report stage differs from actual report")
    supporting = evidence.get("supportingEvidence")
    if supporting and check_artifacts:
        evidence_file(root, supporting.get("path"), supporting.get("sha256"))
        if supporting.get("rawNativeLogPath"):
            evidence_file(root, supporting["rawNativeLogPath"], supporting.get("rawNativeLogSha256"))


def check_reference(root, evidence, selected, *, check_artifacts):
    if not evidence.get("reportPath"):
        return
    reference = json.loads((root / "tools/chrome-extension-runtime/chrome-reference.json").read_text())
    require(evidence.get("runtime") == {
        "version": reference["browserVersion"], "platform": reference["platform"],
        "binarySha256": reference["binary"]["sha256"]}, "Chrome observation differs from the fixed official reference")
    if check_artifacts:
        report = json.loads(evidence_file(root, evidence["reportPath"], evidence.get("reportSha256")).read_text())
        require(all(report.get("reference", {}).get(key) == reference[key]
                    for key in ("browserVersion", "platform", "package", "binary", "vendorMetadata")),
                "Actual Chrome report differs from the frozen vendor-verified reference")
        item = report_item(report, selected["id"])
        keys = ("version",) if evidence.get("evidenceKind") == "ui-inventory" else ("version", "packageSha256")
        require(all(item.get(key) == selected.get(key) for key in keys),
                "Actual reference report differs from the frozen extension package")
        if evidence.get("operationStatus") == "primary-passed":
            adapter = next((adapter for adapter in SUITES.values() if selected["id"] in adapter["ids"]), None)
            if adapter is not None:
                check_reference_primary(root, selected, adapter, report, item)
            else:
                # These historical Chrome-only families cannot qualify any native
                # row until a source-specific native adapter is added above.
                capture_ids = {"nlipoenfbbikpbjkfpfillcgkoblgpmj", "fdpohaocaechififmbbbbbknoalclacl"}
                media = item.get("media", {})
                capture = selected["id"] in capture_ids and media.get("pixel") == [41, 126, 216, 255] and \
                    media.get("width", 0) > 0 and media.get("height", 0) > 0 and item.get("steps")
                require(report.get("stage") == "actual-package-primary-operation" and
                        item.get("primaryOperationResult") in {"pass", "passed"} and
                        not item.get("error") and (item.get("checks") or capture),
                        "Actual Chrome item did not complete its recorded primary")


def check_reference_primary(root, row, adapter, report, item):
    kind = adapter["kind"]
    stage = {
        "devtools": "actual-package-devtools-primary-operation",
        "sidepanel": "actual-sidepanel-primary-operation",
        "automation": "frozen-automation-primary-operation",
        "webscraper": "frozen-webscraper-primary-operation",
        "bitwarden": "actual-Bitwarden-primary-operation",
    }.get(kind, "actual-package-primary-operation")
    if item["id"] == "jnhgnonknehpejjnehehllkliplmbmhn":
        stage = "frozen-webscraper-primary-operation"
    require(report.get("stage") == stage, "Actual Chrome primary adapter stage mismatch")
    require(all(item.get(key) == row.get(key) for key in ("id", "version", "packageSha256")),
            "Actual Chrome primary item differs from the selected frozen package")
    check_primary_observables(root, row, adapter, report, item, native=False)


def validate(root, registry, *, check_artifacts=False, candidate=None, candidate_module=None,
             candidate_manifest_sha=None):
    directory = root / "tools/chrome-extension-runtime"
    lock_bytes = (directory / "corpus-lock.json").read_bytes()
    lock = json.loads(lock_bytes)
    rows = registry.get("extensions", [])
    selection = registry.get("selection", {})
    require(len(rows) == len(lock["extensions"]) == 50, "All50 frozen rows must remain present")
    runtime_lock = json.loads((root / "floorp-runtime.lock.json").read_text())
    require(registry.get("targetRuntime", {}).get("commit") == runtime_lock["source"]["commit"] and
            all(artifact["version"] == registry["targetRuntime"].get("version") for artifact in runtime_lock["artifacts"]),
            "Qualification target differs from the pinned Runtime source/version")
    require(selection.get("lockPath") == "tools/chrome-extension-runtime/corpus-lock.json" and
            selection.get("lockSha256") == hashlib.sha256(lock_bytes).hexdigest() and
            selection.get("selectionSha256") == lock["selectionSha256"], "Frozen selection digest changed")
    require(selection.get("selectedDenominator") == 50 and selection.get("unavailableIncludedInDenominator") is True,
            "Unavailable rows must remain in the50 denominator")
    require(selection.get("frozenPackages") == 46 and selection.get("unavailablePackages") == 4 and
            sum(row.get("packageStatus") == "unavailable" for row in rows) == 4,
            "Exactly4 unavailable selections must remain visible")
    for browser in ("chrome", "floorp"):
        require(registry["operationStatusCounts"][browser] == dict(Counter(
            row[browser]["operationStatus"] for row in rows)), "Operation count mismatch")
    native_rows, groups = [], Counter()
    for index, (selected, row) in enumerate(zip(lock["extensions"], rows), 1):
        require(row.get("index") == index, "Frozen selection index changed")
        for key in ("id", "name", "primaryOperation"):
            require(row.get(key) == selected[key], "Frozen order/identity/primary changed: " + key)
        require(row.get("version") == selected.get("version") and row.get("packageSha256") == selected.get("packageSha256") and
                row.get("packageStatus") == selected["status"], "Frozen package metadata changed")
        require(row.get("owner") and row.get("nextStep") and row.get("blockerCategory"), "Missing audit ownership/blocker")
        require(isinstance(row.get("nativeQualified"), bool), "Row native qualification must be explicit")
        require(row.get("nativeQualified") is row["floorp"].get("nativeQualified"), "Row/native evidence flags differ")
        for previous in row.get("history", []):
            require(previous.get("browser") in {"chrome", "floorp"} and previous.get("supersededReason"),
                    "Historical observation must identify its browser and replacement reason")
            observation = previous.get("observation", {})
            require(observation.get("nativeQualified") is False, "Historical observations cannot add native qualification")
            check_observation(root, observation, check_artifacts=check_artifacts)
            if previous["browser"] == "chrome":
                check_reference(root, observation, selected, check_artifacts=check_artifacts)
        for browser in ("chrome", "floorp"):
            evidence = row[browser]
            check_observation(root, evidence, check_artifacts=check_artifacts)
            require(browser != "chrome" or evidence.get("nativeQualified") is False, "Chrome cannot qualify native Floorp")
            if browser == "chrome":
                check_reference(root, evidence, selected, check_artifacts=check_artifacts)
            path = evidence.get("reportPath")
            if browser == "floorp" and path and evidence.get("buildScope") == HISTORICAL_SCOPE:
                require(evidence["runtime"].get("version") == "157.0" and evidence.get("nativeQualified") is False,
                        "Historical157 evidence cannot qualify a native candidate")
            if browser == "floorp" and evidence.get("buildScope") == NATIVE_SCOPE:
                report, item, identity, runtime, adapter, run_id = candidate_binding(root, evidence, selected, candidate,
                                                                  candidate_module, candidate_manifest_sha)
                if evidence.get("nativeQualified") is True:
                    require(selected["status"] == "frozen" and
                            not str(row.get("primaryOperationApplicability", "")).startswith("not-applicable"),
                            "Unavailable/inapplicable frozen primary cannot qualify")
                    require(evidence.get("operationStatus") == "primary-passed" and
                            evidence.get("evidenceKind") == "actual-package-primary-operation" and
                            evidence.get("primaryOperationAttempted") is True, "Only completed actual primaries qualify")
                    require(row["chrome"].get("operationStatus") == "primary-passed",
                            "Native primary requires a genuine completed Chrome reference")
                    # Native positives always verify their reference artifact too,
                    # even when historical artifact verification is not requested.
                    check_observation(root, row["chrome"], check_artifacts=True)
                    check_reference(root, row["chrome"], selected, check_artifacts=True)
                    reference_report = json.loads((root / row["chrome"]["reportPath"]).read_text())
                    reference_item = report_item(reference_report, selected["id"])
                    check_reference_primary(root, row, adapter, reference_report, reference_item)
                    check_primary_observables(root, row, adapter, report, item)
                    require(item.get("version") == selected.get("version") and
                            item.get("packageSha256") == selected.get("packageSha256"), "Actual frozen package differs")
                    expected = original_inventory(root, selected)
                    background = item.get("conversion", {}).get("background", item.get("background", item.get("nativeEvidence", {}).get("background")))
                    check_background(item.get("nativeEvidence"), item.get("sourceIntegrity"), background,
                                     expected, identity, runtime, run_id)
                    require(evidence.get("recordedScriptIntegrity") == {
                        key: expected[key] for key in ("unchangedScriptCount", "unchangedScriptsSha256")},
                        "Registry script evidence differs from immutable frozen archive")
                    native_rows.append(row["id"])
                    groups[(identity["platform"], identity["buildId"], identity["packageSha256"])] += 1
            elif browser == "floorp":
                require(evidence.get("nativeQualified") is False, "Unscoped or historical evidence cannot qualify")
    require(type(registry.get("nativeQualifiedCount")) is int and registry["nativeQualifiedCount"] == len(native_rows),
            "Qualified count must derive from actual validated rows")
    # Do not infer cross-OS release qualification from one platform's corpus rows.
    require(registry.get("nativeQualified") is False, "Release qualification needs a separate reviewed3-OS policy")
    require(registry.get("primaryOperationPassRate") is None, "No release/compatibility percentage asserted by this registry")
    return {"selected": 50, "unavailableIncluded": 4, "nativeQualifiedCount": len(native_rows),
            "candidateGroups": [{"platform": key[0], "buildId": key[1], "packageSha256": key[2], "count": value}
                                for key, value in sorted(groups.items())]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--registry", type=Path)
    parser.add_argument("--check-artifacts", action="store_true")
    parser.add_argument("--candidate-manifest", type=Path)
    parser.add_argument("--browser", type=Path)
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    registry_path = args.registry or root / "tools/chrome-extension-runtime/qualification-evidence.json"
    registry = json.loads(registry_path.read_text())
    candidate = module = manifest_sha = None
    require(bool(args.candidate_manifest) == bool(args.browser), "Supply both candidate manifest and browser")
    if args.candidate_manifest:
        spec = importlib.util.spec_from_file_location("candidate_identity", root / "tools/chrome-extension-runtime/candidate-identity.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        candidate = module.authenticate_candidate(args.candidate_manifest, args.browser, root)
        manifest_sha = sha(args.candidate_manifest)
    result = validate(root, registry, check_artifacts=args.check_artifacts, candidate=candidate,
                      candidate_module=module, candidate_manifest_sha=manifest_sha)
    print("PASS qualification evidence " + json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError, KeyError, TypeError, zipfile.BadZipFile) as error:
        print("FAIL qualification evidence: " + str(error), file=sys.stderr)
        raise SystemExit(1) from error
