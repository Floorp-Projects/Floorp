from pathlib import Path
r=Path('/workspace/qa-linux-22/C/KAN-90/version-comparison')
s=Path('/workspace/qa-linux-22/C/KAN-90/thunderbird-origin/run-flatpak-159.py').read_text()
s=s.replace("R=Path('/workspace/qa-linux-22/C/KAN-90');O=Path(__file__).resolve().parent/'flatpak-159-default';O.mkdir(exist_ok=True)","R=Path('/workspace/qa-linux-22/C/KAN-90'); V=Path(__file__).resolve().parent; variant=sys.argv[1]; browser=Path(sys.argv[2]).resolve(); O=V/variant;O.mkdir(exist_ok=True)")
s=s.replace("MOZ_APP_REMOTINGNAME']='qa-kan90-floorp'","MOZ_APP_REMOTINGNAME']='qa-kan90-compare-'+variant")
s=s.replace("'flatpak-159-default'","variant")
s=s.replace("str(R/'flatpak-env.sh')+' flatpak run --user one.ablaze.floorp --profile '+str(Path(os.environ['HOME'])/'.var/app/one.ablaze.floorp/qa-tb-flatpak-route-profile')", "str(V/('launch-'+variant+'.sh'))+' --profile '+str(V/(variant+'-floorp-profile'))")
s=s.replace("pp=Path(os.environ['HOME'])/'.var/app/one.ablaze.floorp/qa-tb-flatpak-route-profile'", "pp=V/(variant+'-floorp-profile')")
s=s.replace("['flatpak','run','--user','one.ablaze.floorp','--profile',str(pp)", "[str(browser),'--profile',str(pp)")
s=s.replace("tp=Path(os.environ['HOME'])/'.var/app/org.mozilla.thunderbird/qa-origin-profile'", "tp=Path(os.environ['HOME'])/('.var/app/org.mozilla.thunderbird/qa-comparison-'+variant)")
s=s.replace("'sourceSha':'984509d2053be7bae9027133bda7259cd706b208'", "'sourceSha':('38bde3075f197b9a88490875d1534bb79d6159a5' if variant=='12.17.2' else '984509d2053be7bae9027133bda7259cd706b208'),'browserDistribution':'official release Linux x86_64 archive','variant':variant")
s=s.replace("result['floorpStart']=fm.js('return {pid:Services.appinfo.processID,version:Services.appinfo.version, tabs:gBrowser.tabs.length};')", "result['floorpStart']=fm.js('return {pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref(\"security.sandbox.content.level\"),tabs:gBrowser.tabs.length};')")
(r/'run-comparison.py').write_text(s)
(r/'env.sh').write_text('''#!/bin/bash
set -eu
qa90=/workspace/qa-linux-22/C/KAN-90
qaCompare="$qa90/version-comparison"
export HOME="$qaCompare/home"
export XDG_DATA_HOME="$HOME/.local/share" XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_RUNTIME_DIR="$qaCompare/runtime"
export LD_LIBRARY_PATH="$qa90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu"
export PATH="$qa90/sysroot/usr/bin:/workspace/.floorp-cloud/bin:$PATH"
export FLATPAK_DBUSPROXY="$qa90/sysroot/usr/bin/xdg-dbus-proxy"
exec "$@"
''')
for variant,browser in [('12.21.1','/workspace/qa-linux-22/C/products/floorp/floorp'),('12.17.2',str(r/'products/12.17.2/floorp/floorp'))]:
 p=r/('launch-'+variant+'.sh');p.write_text('#!/bin/bash\nset -eu\nexport MOZ_APP_REMOTINGNAME=qa-kan90-compare-'+variant+'\nexec "'+str(r/'env.sh')+'" "'+browser+'" "$@"\n');p.chmod(0o755)
(r/'env.sh').chmod(0o755)
import ast
ast.parse(s)
