# SPDX-License-Identifier: MPL-2.0
"""Shared, credential-free preparation for isolated Desktop Sync QA apps."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile
from urllib.parse import urlsplit
import zipfile


# These locations are verified against the full Gecko 159 Linux distribution.
# Source paths and archive entry names deliberately remain separate.
LIFECYCLE_RESOURCES = {
    "toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs": (
        "omni.ja", "moz-src/toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs"),
    "toolkit/components/extensions/parent/ext-contextualIdentities.js": (
        "omni.ja", "chrome/toolkit/content/extensions/parent/ext-contextualIdentities.js"),
    "browser/components/preferences/config/containers.mjs": (
        "browser/omni.ja", "chrome/browser/content/browser/preferences/config/containers.mjs"),
    "browser/components/preferences/config/tabs-browsing.mjs": (
        "browser/omni.ja", "chrome/browser/content/browser/preferences/config/tabs-browsing.mjs"),
}
SERVICE_RESOURCE = "modules/services-sync/service.sys.mjs"
EXTERNAL_OPEN_RESOURCES = {
    "browser/components/BrowserContentHandler.sys.mjs": (
        "omni.ja", "moz-src/browser/components/BrowserContentHandler.sys.mjs"),
    "browser/modules/BrowserDOMWindow.sys.mjs": (
        "browser/omni.ja", "modules/BrowserDOMWindow.sys.mjs"),
}


def generated_entries(directory, archive_prefix):
    entries = {}
    directory = directory.resolve(strict=True)
    for line in (directory / "jar.mn").read_text().splitlines():
        match = re.fullmatch(r"\s+(\S+) \(([^)]+)\)", line)
        if not match:
            continue
        destination, source = match.groups()
        destination_path = PurePosixPath(destination)
        source_path = (directory / source).resolve(strict=True)
        if (destination_path.is_absolute() or ".." in destination_path.parts or
                not source_path.is_relative_to(directory) or not source_path.is_file()):
            raise ValueError("Generated archive entry escapes its build directory")
        target = archive_prefix + destination
        if target in entries:
            raise ValueError("Duplicate generated archive entry")
        entries[target] = source_path.read_bytes()
    if not entries:
        raise ValueError(f"Missing production entries in {directory}")
    return entries


def app_layout(app, platform):
    if platform == "macos":
        return app / "Contents/Resources", app / "Contents/MacOS/floorp"
    if platform == "linux":
        return app, app / "floorp"
    raise ValueError("Only complete macOS and Linux Floorp apps are supported")


def validate_source_app(app, platform):
    resources, binary = app_layout(app, platform)
    if not binary.is_file() or not os.access(binary, os.X_OK):
        raise ValueError("Source must contain an executable complete Floorp app")
    for relative in ("omni.ja", "browser/omni.ja", "platform.ini"):
        if not (resources / relative).is_file():
            raise ValueError(f"Source app lacks {relative}")
    with zipfile.ZipFile(resources / "omni.ja") as archive:
        if SERVICE_RESOURCE not in archive.namelist():
            raise ValueError("Source app lacks native Sync resources")
    if platform == "macos" and not (app / "Contents/Info.plist").is_file():
        raise ValueError("Source must be a complete macOS bundle")
    return resources, binary


def copy_app(source, destination, platform):
    if platform == "macos":
        subprocess.run(["ditto", str(source), str(destination)], check=True)
    else:
        shutil.copytree(source, destination, symlinks=True)


def _patched_resources(resources, patch, mapping, scratch, previous_patch=None):
    with tempfile.TemporaryDirectory(prefix="native-patch-", dir=scratch) as temporary:
        root = Path(temporary)
        # QA output commonly lives below the Floorp checkout. Without its own
        # repository, git apply treats this as a subdirectory and silently
        # skips every patch path outside that prefix, even with --check.
        subprocess.run(["git", "init", "--quiet"], cwd=root, check=True)
        for source, (archive_path, entry) in mapping.items():
            with zipfile.ZipFile(resources / archive_path) as archive:
                payload = archive.read(entry)
            target = root / source
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(payload)
        reverse = subprocess.run(["git", "apply", "--reverse", "--check", str(patch)],
                                 cwd=root, capture_output=True)
        if reverse.returncode:
            forward = subprocess.run(["git", "apply", "--check", str(patch)],
                                     cwd=root, capture_output=True)
            if forward.returncode and previous_patch is not None:
                old = root / "previous.patch"
                old.write_bytes(previous_patch)
                subprocess.run(["git", "apply", "--reverse", "--check", str(old)], cwd=root, check=True)
                subprocess.run(["git", "apply", "--reverse", str(old)], cwd=root, check=True)
            elif forward.returncode:
                raise ValueError("Native resources match neither the current patch nor pristine Runtime; "
                                 "supply --source-ref for the exact source app release commit")
            subprocess.run(["git", "apply", "--check", str(patch)], cwd=root, check=True)
            subprocess.run(["git", "apply", str(patch)], cwd=root, check=True)
        subprocess.run(["git", "apply", "--reverse", "--check", str(patch)], cwd=root, check=True)
        return {(archive, entry): (root / source).read_bytes()
                for source, (archive, entry) in mapping.items()}


def overlay_app(resources, repository, generated, scratch, source_ref=None):
    replacements = {("omni.ja", name): payload for name, payload in generated.items()}
    generated_roots = tuple({
        "chrome/noraneko/" + name.removeprefix("chrome/noraneko/").split("/", 1)[0] + "/"
        for name in generated if name.startswith("chrome/noraneko/")
    } | {"chrome/noraneko/nora-content/", "chrome/noraneko/nora-resource/"})
    replacements.update(_patched_resources(
        resources, repository / "tools/patches/modules-services-sync-service.sys.patch",
        {SERVICE_RESOURCE: ("omni.ja", SERVICE_RESOURCE)}, scratch))
    lifecycle = repository / ".github/patches/floorp-runtime/common/workspace-container-lifecycle.patch"
    if lifecycle.is_file():
        replacements.update(_patched_resources(resources, lifecycle, LIFECYCLE_RESOURCES, scratch))
    elif any(name.endswith("/WorkspaceContainerBindings.sys.mjs") for name in generated):
        raise ValueError("Container bindings require the native lifecycle patch")
    external_relative = ".github/patches/floorp-runtime/common/workspace-external-containers.patch"
    external = repository / external_relative
    if external.is_file():
        previous = None
        if source_ref:
            previous = subprocess.check_output(
                ["git", "show", f"{source_ref}:{external_relative}"], cwd=repository)
        replacements.update(_patched_resources(resources, external, EXTERNAL_OPEN_RESOURCES, scratch, previous))
    override_script = repository / "static/gecko/pref/override.sh"
    if override_script.is_file():
        with tempfile.TemporaryDirectory(prefix="native-prefs-", dir=scratch) as temporary:
            target = Path(temporary) / "firefox.js"
            with zipfile.ZipFile(resources / "browser/omni.ja") as archive:
                target.write_bytes(archive.read("defaults/preferences/firefox.js"))
            subprocess.run(["bash", str(override_script), str(target)], check=True)
            replacements[("browser/omni.ja", "defaults/preferences/firefox.js")] = target.read_bytes()
    archives = sorted({archive for archive, _ in replacements})
    for relative in archives:
        archive = resources / relative
        desired = {entry: payload for (target, entry), payload in replacements.items() if target == relative}
        temporary = archive.with_name("omni.qa.tmp")
        remaining = dict(desired)
        with zipfile.ZipFile(archive) as original, zipfile.ZipFile(
                temporary, "w", compression=zipfile.ZIP_DEFLATED) as destination:
            for entry in original.infolist():
                if relative == "omni.ja" and entry.filename.startswith(generated_roots):
                    continue
                destination.writestr(entry, remaining.pop(entry.filename, original.read(entry.filename)))
            for name, contents in remaining.items():
                destination.writestr(name, contents)
        temporary.replace(archive)
        with zipfile.ZipFile(archive) as verified:
            for entry, payload in desired.items():
                if verified.read(entry) != payload:
                    raise ValueError("Production resource verification failed")
    return {f"{archive}:{entry}": hashlib.sha256(payload).hexdigest()
            for (archive, entry), payload in replacements.items()}


def proxy_preferences(proxy_url):
    if not proxy_url:
        return {}
    try:
        proxy = urlsplit(proxy_url)
        port = proxy.port or 80
    except ValueError as error:
        raise ValueError("Invalid QA proxy URL") from error
    if (proxy.scheme != "http" or not proxy.hostname or proxy.username is not None or
            proxy.password is not None or proxy.query or proxy.fragment or proxy.path not in ("", "/")):
        raise ValueError("QA proxy must be an HTTP proxy URL without credentials, query, or path")
    return {
        "network.proxy.type": 1,
        "network.proxy.http": proxy.hostname,
        "network.proxy.http_port": port,
        "network.proxy.ssl": proxy.hostname,
        "network.proxy.ssl_port": port,
        "network.proxy.share_proxy_settings": True,
        "network.proxy.no_proxies_on": "localhost, 127.0.0.1, ::1",
        "network.proxy.allow_hijacking_localhost": False,
    }


def certificate_files(explicit):
    paths = explicit or ([Path(os.environ["NODE_EXTRA_CA_CERTS"])]
                         if os.environ.get("NODE_EXTRA_CA_CERTS") else [])
    result = []
    for value in paths:
        path = Path(value).resolve(strict=True)
        content = path.read_bytes()
        if b"PRIVATE KEY" in content or content.count(b"-----BEGIN CERTIFICATE-----") != 1:
            raise ValueError("Each QA CA file must contain one public PEM certificate and no private key")
        result.append(path)
    if result and not shutil.which("certutil"):
        raise ValueError("certutil is required to install the QA proxy CA before browser startup")
    return result


def install_profile_certificates(profile, certificates):
    if not certificates:
        return []
    database = f"sql:{profile}"
    subprocess.run(["certutil", "-N", "--empty-password", "-d", database], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    hashes = []
    for certificate in certificates:
        digest = hashlib.sha256(certificate.read_bytes()).hexdigest()
        subprocess.run(["certutil", "-A", "-d", database, "-n", f"Floorp QA CA {digest[:16]}",
                        "-t", "C,,", "-i", str(certificate)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        hashes.append(digest)
    return hashes


def profile_preferences(name, port, proxy):
    return {
        "floorp.workspaces.sync.acceptance-profile": name,
        "marionette.port": port,
        "remote.prefs.recommended": False,
        "services.sync.registerEngines": "FloorpWorkspaces",
        "services.sync.engine.floorp-workspaces": True,
        "services.sync.engineStatusChanged.floorp-workspaces": True,
        "services.sync.engine.floorp-workspaces-v2": True,
        "services.sync.engineStatusChanged.floorp-workspaces-v2": True,
        "services.sync.client.name": f"Floorp Workspace Sync QA {name}",
        "browser.shell.checkDefaultBrowser": False,
        "floorp.browser.welcome.page.shown": True,
        "browser.startup.homepage_override.mstone": "ignore",
        "app.update.auto": False,
        "signon.rememberSignons": False,
        **proxy,
    }


def write_preferences(profile, prefs):
    (profile / "user.js").write_text("\n".join(
        f"user_pref({json.dumps(key)}, {json.dumps(value)});" for key, value in prefs.items()) + "\n")
