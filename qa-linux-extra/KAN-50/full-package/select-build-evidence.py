#!/usr/bin/env python3
"""Allowlist only public patch/provenance observations from a local CI log."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('log',type=Path);a=p.parse_args();b=a.log.read_bytes();lines=b.decode(errors='replace').splitlines();selected=[]
needles=['picture-in-picture-app-window.patch','moz-src-toolkit-components-pictureinpicture-PictureInPicture.sys.patch','Verified exact stable v2 installer/package:']
for i,l in enumerate(lines):
 if any(n in l for n in needles) and '?' not in l and 'token' not in l.lower() and 'secret' not in l.lower() and 'echo ' not in l:
  selected.append({'line':i+1,'text':l[:2000]})
print(json.dumps({'runId':42086912660,'jobId':127454399181,'logSha256':hashlib.sha256(b).hexdigest(),'selectedEvidence':selected,'fullLogExported':False},indent=2))
