#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Verify generated PR jar entries against reachable packaged chrome registrations.
Read-only, narrow normal Floorp jar.mn grammar. Unsupported grammar fails closed.
This proves byte/registration integrity, not browser startup or native tests.
"""
import hashlib
from pathlib import Path, PurePosixPath
import posixpath
import re
import zipfile

MOUNTS=('content','startup','skin','resource','pages-newtab','pages-settings','pages-welcome',
        'pages-notes','pages-modal-child','pages-profile-manager','pages-aboutDialog')


def safe_join(parent, relative):
    if not relative or relative.startswith('/') or ':' in relative or '\\' in relative:
        raise ValueError('unsupported absolute/URL registration')
    result=posixpath.normpath(posixpath.join(parent,relative))
    if result=='..' or result.startswith('../'):
        raise ValueError('registration escapes archive')
    return result


def generated(directory, project):
    directory=Path(directory).resolve(strict=True);project=Path(project).resolve(strict=True)
    if not directory.is_relative_to(project):raise ValueError('generated directory escapes candidate')
    registrations=[];entries={};overrides=[]
    lines=(directory/'jar.mn').read_text().splitlines()
    if not lines or lines[0]!='noraneko.jar:':raise ValueError('unsupported generated jar header')
    for line in lines[1:]:
        line=line.strip()
        if not line or line.startswith('#'):continue
        if line.startswith('% '):
            fields=line[2:].split()
            if fields[0]=='override':
                if len(fields)!=3 or not fields[1].startswith('chrome://') or not fields[2].startswith('chrome://noraneko'):
                    raise ValueError('unsupported generated override')
                overrides.append((fields[1],fields[2]));continue
            if len(fields)!=4 or fields[0] not in {'content','resource'} or not fields[2].startswith('%') or fields[3]!='contentaccessible=yes':
                raise ValueError('unsupported generated registration')
            registrations.append((fields[0],fields[1],fields[2][1:]))
            continue
        match=re.fullmatch(r'(\S+) \(([^)]+)\)',line)
        if not match:raise ValueError('unsupported generated jar entry')
        target,source=match.groups();safe_join('',target)
        source_path=(directory/source).resolve(strict=True)
        if not source_path.is_relative_to(directory) or not source_path.is_file() or target in entries:
            raise ValueError('unsafe/duplicate generated entry')
        entries[target]=hashlib.sha256(source_path.read_bytes()).hexdigest()
    if len(registrations)!=1 or not entries:raise ValueError('missing/ambiguous generated registration')
    kind,name,prefix=registrations[0]
    if not prefix.endswith('/') or any(not path.startswith(prefix) for path in entries):
        raise ValueError('generated files outside registered prefix')
    return kind,name,prefix,entries,overrides


def registrations(archive, override_rows):
    names=archive.namelist()
    if len(names)!=len(set(names)):raise ValueError('duplicate packed entries')
    rows={};visited=set()
    roots=[n for n in ('chrome.manifest','chrome/chrome.manifest') if n in names]
    if not roots:return rows
    def visit(name):
        if name in visited:return
        visited.add(name)
        if len(visited)>500:raise ValueError('manifest graph budget')
        data=archive.read(name)
        if len(data)>4*1024**2:raise ValueError('manifest size budget')
        parent=posixpath.dirname(name)
        for line in data.decode().splitlines():
            fields=line.split('#',1)[0].split()
            if not fields:continue
            if fields[0]=='manifest':
                if len(fields)!=2:raise ValueError('conditional manifest include unsupported')
                visit(safe_join(parent,fields[1]))
            elif fields[0]=='override' and len(fields)>=3 and fields[2].startswith('chrome://noraneko'):
                if len(fields)!=3 or fields[1] in override_rows:raise ValueError('conditional/duplicate packed override')
                override_rows[fields[1]]=fields[2]
            elif fields[0] in {'content','resource'} and len(fields)>1 and fields[1].startswith('noraneko'):
                if len(fields) not in {3,4} or (len(fields)==4 and fields[3]!='contentaccessible=yes'):
                    raise ValueError('conditional/unsupported Noraneko registration')
                key=(fields[0],fields[1]);value=safe_join(parent,fields[2])+'/'
                if key in rows:raise ValueError('duplicate Noraneko registration')
                rows[key]=value
    for name in roots:visit(name)
    return rows


def verify(project, app):
    project=Path(project).resolve(strict=True);app=Path(app).resolve(strict=True)
    expected=[generated(project/'_dist/noraneko'/mount,project) for mount in MOUNTS]
    if len({(v[0],v[1]) for v in expected})!=len(expected):raise ValueError('duplicate expected namespace')
    found={};assets=[];override_rows={}
    for relative in ('omni.ja','browser/omni.ja'):
        path=app/relative
        if path.is_symlink():raise ValueError('symlinked packed archive')
        with zipfile.ZipFile(path) as archive:
            rows=registrations(archive,override_rows)
            for kind,name,prefix,entries,overrides in expected:
                key=(kind,name)
                if key not in rows:continue
                if key in found:raise ValueError('registration shadowed across packed archives')
                found[key]={'archive':relative,'base':rows[key]}
                for target,digest in entries.items():
                    actual=rows[key]+target[len(prefix):]
                    data=archive.read(actual)
                    if hashlib.sha256(data).hexdigest()!=digest:raise ValueError('packed PR asset differs: '+target)
                    assets.append({'archive':relative,'path':actual,'sha256':digest})
    if set(found)!={(v[0],v[1]) for v in expected}:raise ValueError('missing reachable production registrations')
    for expected_group in expected:
        for source,target in expected_group[4]:
            if override_rows.get(source)!=target:raise ValueError('missing/wrong packed PR override')
    return {'schema':'floorp-packed-pr-registration-v1','status':'PACKED_PR_BYTES_VERIFIED',
            'registrations':[{'kind':k[0],'namespace':k[1],**v} for k,v in sorted(found.items())],
            'assets':assets,'overrides':override_rows,'native_execution':'UNVERIFIED'}
