#!/usr/bin/env python3
"""Validate the macOS signed app/updater ServiceManagement trust contract.

Run on macOS against the final signed .app, before packaging and release.
Never modifies the app; any missing, malformed, or unverifiable input fails.
"""
from __future__ import annotations
import argparse
import plistlib
import re
import subprocess
import sys
from pathlib import Path

UPDATER = "org.mozilla.updater"
DMG_HELPER = "org.mozilla.dmgInstallHelper"
PRODUCTION_TEAM = "DV2U35YBHT"


class ValidationError(Exception):
    pass


def run(*args: str) -> str:
    try:
        p = subprocess.run(
            args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
        )
    except OSError as exc:
        raise ValidationError(f"cannot execute {args[0]}: {exc}") from exc
    if p.returncode:
        raise ValidationError(
            f'{args[0]} failed ({p.returncode}): {p.stderr.decode(errors="replace").strip()}'
        )
    return p.stdout.decode("utf-8", errors="strict")


def architectures(binary: Path) -> set[str]:
    result = run("/usr/bin/lipo", "-archs", str(binary)).split()
    if (
        not result
        or len(result) != len(set(result))
        or any(not re.fullmatch(r"[A-Za-z0-9_]+", a) for a in result)
    ):
        raise ValidationError(f"invalid architecture list for {binary}")
    return set(result)


def embedded_plist(binary: Path, arch: str) -> dict:
    output = run(
        "/usr/bin/otool",
        "-arch",
        arch,
        "-X",
        "-s",
        "__TEXT",
        "__info_plist",
        str(binary),
    )
    chunks = []
    for line in output.splitlines():
        fields = line.split()
        # Section dump lines begin with a hexadecimal address; reject other payload.
        if not fields or not re.fullmatch(r"[0-9a-fA-F]{8,16}", fields[0]):
            continue
        if len(fields) < 2 or any(
            not re.fullmatch(r"(?:[0-9a-fA-F]{2}){1,8}", s) for s in fields[1:]
        ):
            raise ValidationError(f"malformed otool output for {binary} ({arch})")
        for field in fields[1:]:
            raw = bytes.fromhex(field)
            # otool prints ARM64 32-bit words in host integer byte order; x86_64 prints bytes.
            if arch.startswith("arm64") and len(raw) == 4:
                raw = raw[::-1]
            elif len(raw) != 1:
                raise ValidationError(
                    f"unsupported otool byte grouping for {binary} ({arch})"
                )
            chunks.append(raw.hex())
    try:
        data = bytes.fromhex("".join(chunks)).rstrip(b"\x00")
        value = plistlib.loads(data)
    except (ValueError, plistlib.InvalidFileException, OverflowError) as exc:
        raise ValidationError(
            f"invalid embedded plist for {binary} ({arch}): {exc}"
        ) from exc
    if not isinstance(value, dict):
        raise ValidationError(
            f"embedded plist is not a dictionary for {binary} ({arch})"
        )
    return value


def validate_requirement(req: object) -> str:
    if not isinstance(req, str) or not req.strip():
        raise ValidationError("missing or invalid code requirement")
    # Compile each candidate, including alternatives that do not happen to match.
    run("/usr/bin/csreq", "-r", "=" + req, "-t")
    return req


def require_exact_policy(actual: str, expected: str) -> None:
    actual_canonical = run("/usr/bin/csreq", "-r", "=" + actual, "-t").strip()
    expected_canonical = run("/usr/bin/csreq", "-r", "=" + expected, "-t").strip()
    if not actual_canonical or actual_canonical != expected_canonical:
        raise ValidationError(
            "embedded requirement differs from pinned production policy"
        )


def production_requirement(identifier: object) -> str:
    if not isinstance(identifier, str) or not re.fullmatch(
        r"[A-Za-z0-9][A-Za-z0-9.-]*", identifier
    ):
        raise ValidationError("invalid signing identifier")
    return (
        f'identifier "{identifier}" and anchor apple generic and '
        "certificate 1[field.1.2.840.113635.100.6.2.6] and "
        "certificate leaf[field.1.2.840.113635.100.6.1.13] and "
        f'certificate leaf[subject.OU] = "{PRODUCTION_TEAM}"'
    )


def verify_signature(path: Path) -> None:
    run(
        "/usr/bin/codesign",
        "--verify",
        "--strict",
        "--all-architectures",
        "--verbose=2",
        str(path),
    )


def matches(path: Path, arch: str, requirements: list[str]) -> None:
    errors = []
    for req in requirements:
        try:
            run(
                "/usr/bin/codesign",
                "--verify",
                "--strict",
                "--architecture",
                arch,
                "-R=" + req,
                str(path),
            )
            return
        except ValidationError as exc:
            errors.append(str(exc))
    raise ValidationError(
        f'no code requirement matched {path} ({arch}): {"; ".join(errors)}'
    )


def required_file(path: Path) -> Path:
    if not path.is_file():
        raise ValidationError(f"missing file: {path}")
    return path


def validate(app: Path, expected_bundle_id: str) -> None:
    if not app.is_dir():
        raise ValidationError(f"app bundle not found: {app}")
    info_path = required_file(app / "Contents/Info.plist")
    try:
        with info_path.open("rb") as stream:
            info = plistlib.load(stream)
    except (OSError, ValueError, plistlib.InvalidFileException) as exc:
        raise ValidationError(f"invalid app Info.plist: {exc}") from exc
    if not isinstance(info, dict):
        raise ValidationError("app Info.plist is not a dictionary")
    if info.get("CFBundleIdentifier") != expected_bundle_id:
        raise ValidationError(
            "app CFBundleIdentifier differs from expected release identity"
        )
    executable = info.get("CFBundleExecutable")
    if (
        not isinstance(executable, str)
        or not executable
        or Path(executable).name != executable
        or executable in (".", "..")
    ):
        raise ValidationError("invalid CFBundleExecutable")
    app_bin = required_file(app / "Contents/MacOS" / executable)
    app_archs = architectures(app_bin)
    verify_signature(app)
    app_req = validate_requirement(
        production_requirement(info.get("CFBundleIdentifier"))
    )
    for arch in sorted(app_archs):
        matches(app, arch, [app_req])
    updater_app = app / "Contents/MacOS/updater.app"
    if not updater_app.is_dir():
        raise ValidationError(f"missing updater app: {updater_app}")
    verify_signature(updater_app)
    helpers = {
        UPDATER: [
            required_file(updater_app / "Contents/MacOS" / UPDATER),
            required_file(app / "Contents/Library/LaunchServices" / UPDATER),
        ]
    }
    dmg = app / "Contents/Library/LaunchServices" / DMG_HELPER
    if dmg.exists():
        helpers[DMG_HELPER] = [required_file(dmg)]
    privileged = info.get("SMPrivilegedExecutables")
    if not isinstance(privileged, dict):
        raise ValidationError("missing SMPrivilegedExecutables dictionary")
    for name, paths in helpers.items():
        helper_req = validate_requirement(privileged.get(name))
        signed_helper_req = validate_requirement(production_requirement(name))
        for path in paths:
            helper_archs = architectures(path)
            if helper_archs != app_archs:
                raise ValidationError(
                    f"architecture mismatch: app={sorted(app_archs)}, {path}={sorted(helper_archs)}"
                )
            verify_signature(path)
            for arch in sorted(app_archs):
                embedded = embedded_plist(path, arch)
                clients = embedded.get("SMAuthorizedClients")
                if not isinstance(clients, list) or not clients:
                    raise ValidationError(
                        f"missing SMAuthorizedClients in {path} ({arch})"
                    )
                client_reqs = [validate_requirement(item) for item in clients]
                matches(path, arch, [signed_helper_req])
                matches(path, arch, [helper_req])
                matches(app, arch, client_reqs)
                require_exact_policy(helper_req, signed_helper_req)
                for client_req in client_reqs:
                    require_exact_policy(client_req, app_req)
    print(
        f"PASS: macOS ServiceManagement trust validated for {app}; architectures={sorted(app_archs)}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("--expected-bundle-id", required=True)
    args = parser.parse_args()
    try:
        validate(args.app, args.expected_bundle_id)
    except (ValidationError, UnicodeError, OSError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
