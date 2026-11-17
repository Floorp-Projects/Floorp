#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Compile, but do not execute, existing package phases for a Debug artifact trial.
Not a native-result issuer. Requires a separate trusted producer preflight.
"""
import hashlib
from pathlib import Path
import subprocess
import zipfile
import yaml
import build_recipe

DENO_BINDING = """: "${DENO_EXECUTABLE_PATH:?controlled Deno required}"
test "$(command -v deno)" = "$DENO_EXECUTABLE_PATH"
test -f "$DENO_EXECUTABLE_PATH" && test -x "$DENO_EXECUTABLE_PATH"
readonly DENO_EXECUTABLE_PATH
export DENO_EXECUTABLE_PATH
"""

PHASES = (*build_recipe.PHASES, 'Build', 'Package noraneko')


def extract(workflow):
    document = yaml.safe_load(Path(workflow).read_text())
    all_steps = [s for job in document['jobs'].values() for s in job.get('steps', []) if 'run' in s]
    scripts = {}
    for name in PHASES:
        matches = [s['run'] for s in all_steps if s.get('name') == name]
        if len(matches) != 1:
            raise ValueError('missing/ambiguous normal package phase: ' + name)
        script = matches[0]
        for old, new in build_recipe.REPLACEMENTS.items():
            script = script.replace(old, new)
        if '${{' in script:
            raise ValueError('unreviewed workflow expression: ' + name)
        scripts[name] = script
    # Reviewed normalization: remove legacy fixed-location Deno probing.
    deno_script=scripts['Build Noraneko']
    prefix,separator,tail=deno_script.partition('cd ./noraneko\n')
    if not separator or hashlib.sha256(prefix.encode()).hexdigest() != '5e381b7120b1f307f06ef42dd16b35b28a49148a1fed2d49f68ca4acdddc0ff8':
        raise ValueError('unreviewed Deno selection block')
    scripts['Build Noraneko'] = DENO_BINDING + separator + tail
    config = scripts['Mach configure']
    if config.count('ac_add_options --enable-artifact-builds') != 1 or config.count('ac_add_options --disable-tests') != 1:
        raise ValueError('artifact configuration boundary changed')
    # Keep artifact mode. Native test assets come from the producer, not this
    # packaging OBJDIR. Explicit debug configure must agree with the Debug ELF.
    scripts['Mach configure'] = config.replace('ac_add_options --enable-artifact-builds',
        'ac_add_options --enable-artifact-builds\nac_add_options --enable-debug')
    build = scripts['Build']
    if 'MOZ_ARTIFACT_FILE="$ARTIFACT" ./mach build' not in build or 'exact_artifact' not in build:
        raise ValueError('official local artifact build path changed')
    # Require caller to stage the authenticated bytes under the fixed official
    # name, and force the non-legacy exact-file branch. No latest/release lookup.
    scripts['Build'] = '''export PROVENANCE_MODE=v2
export INPUT_PLATFORM=Linux-x86_64 CANON_PLATFORM=Linux-x86_64
export VERIFIED_RUNTIME_ARTIFACT_NAME=floorp-linux-x86_64-moz-artifact
: "${NATIVE_DEBUG_PRIMARY_SHA256:?authenticated primary digest required}"
printf '%s  %s\\n' "$NATIVE_DEBUG_PRIMARY_SHA256" "$HOME/artifacts/floorp-linux-x86_64-moz-artifact.tar.xz" | sha256sum --check --strict
''' + build
    order = ('Apply upstream patches', 'Apply Floorp common Runtime patches',
             'Setup Noraneko', 'Write Versions', 'Copy branding files from runtime',
             'Mach configure', 'Build Noraneko', 'Build', 'Inject Noraneko',
             'Apply Firefox Preferences Override', 'Package noraneko')
    return [(name, scripts[name]) for name in order]


def write(workflow, output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for index, (name, script) in enumerate(extract(workflow)):
        path = output / f'{index:02}.sh'
        path.write_text('#!/usr/bin/env bash\nset -euo pipefail\n' + script)
        subprocess.run(['bash', '-n', str(path)], check=True)
        rows.append({'name': name, 'script': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    return rows


def verify_product(primary_root, assembled_root, injected_xhtml):
    primary_root, assembled_root = Path(primary_root), Path(assembled_root)
    def sha(path):
        h = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                h.update(chunk)
        return h.hexdigest()
    identities = {}
    for name in ('floorp', 'libxul.so'):
        before, after = primary_root / name, assembled_root / name
        if before.is_symlink() or after.is_symlink() or not before.is_file() or not after.is_file():
            raise ValueError('missing/plain ELF boundary: ' + name)
        with before.open('rb') as left, after.open('rb') as right:
            elf_headers = left.read(4) == right.read(4) == b'\x7fELF'
        if not elf_headers or sha(before) != sha(after):
            raise ValueError('engine identity changed during UI assembly: ' + name)
        identities[name] = sha(after)
    with zipfile.ZipFile(assembled_root / 'browser/omni.ja') as z:
        actual = z.read('chrome/browser/content/browser/browser.xhtml')
    if actual != injected_xhtml or b'chrome://noraneko-startup/content/chrome_root.js' not in actual:
        raise ValueError('assembled browser XHTML is not the observed PR injection')
    # This is an integrity prerequisite, not a complete chrome-registration or
    # page-resource proof. Caller must verify those before scheduling native tests.
    return {'status':'ASSEMBLY_INTEGRITY_ONLY', 'elf_sha256':identities,
            'native_execution':'UNVERIFIED', 'chrome_registration':'UNVERIFIED'}
