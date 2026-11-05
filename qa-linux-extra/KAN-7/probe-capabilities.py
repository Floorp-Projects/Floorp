#!/usr/bin/env python3
"""Read-only capability/device inventory; no browser or software-compositor startup."""
import ctypes as c,ctypes.util,json,os,platform,socket,glob,subprocess
from pathlib import Path
r={'os':platform.freedesktop_os_release().get('PRETTY_NAME'),'kernel':platform.release(),'machine':platform.machine(),'uid':os.getuid(),'devicePaths':{},'displaySockets':[],'eglDevices':[]}
for p in ['/dev/dri','/sys/class/drm','/dev/nvidia0','/dev/nvidiactl','/dev/dxg']:
 r['devicePaths'][p]={'exists':Path(p).exists()}
r['renderNodes']=glob.glob('/dev/dri/renderD*')
r['sessionEnvironmentPresence']={k:bool(os.environ.get(k)) for k in ['DISPLAY','WAYLAND_DISPLAY','XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS']}
r['systemBusSocketExists']=Path('/run/dbus/system_bus_socket').exists()
for p in glob.glob('/tmp/.X11-unix/X*')+glob.glob('/run/user/*/wayland-*'):
 if Path(p).is_socket():
  s=socket.socket(socket.AF_UNIX);s.settimeout(1)
  try:s.connect(p);result='connected'
  except OSError as e:result={'errno':e.errno,'message':str(e)}
  finally:s.close()
  r['displaySockets'].append({'path':p,'connect':result})
try:
 lib=c.CDLL(ctypes.util.find_library('EGL'));lib.eglGetProcAddress.argtypes=[c.c_char_p];lib.eglGetProcAddress.restype=c.c_void_p
 q=lib.eglGetProcAddress(b'eglQueryDevicesEXT');s=lib.eglGetProcAddress(b'eglQueryDeviceStringEXT')
 r['eglLibrary']=ctypes.util.find_library('EGL');r['eglQueryDevicesAvailable']=bool(q)
 if q and s:
  query=c.CFUNCTYPE(c.c_uint,c.c_int,c.POINTER(c.c_void_p),c.POINTER(c.c_int))(q);string=c.CFUNCTYPE(c.c_char_p,c.c_void_p,c.c_int)(s)
  ds=(c.c_void_p*16)();n=c.c_int();r['eglQueryDevicesSuccess']=bool(query(16,ds,c.byref(n)));r['eglDeviceCount']=n.value
  for d in ds[:n.value]:
   ex=(string(d,0x3055) or b'').decode();o={'extensions':ex,'softwareDevice':'EGL_MESA_device_software' in ex}
   if 'EGL_EXT_device_drm_render_node' in ex:o['renderNode']=(string(d,0x3377) or b'').decode()
   if 'EGL_EXT_device_drm' in ex.split():o['drmDevice']=(string(d,0x3233) or b'').decode()
   r['eglDevices'].append(o)
except Exception as e:r['eglError']=str(e)
r['packageVersions']=subprocess.run(['dpkg-query','-W','-f=${Package} ${Version}\n','libegl-mesa0','libgl1-mesa-dri','libwayland-client0','libdrm2'],text=True,capture_output=True).stdout.splitlines()
r['conclusion']='Hardware Intel/NVIDIA rendering cannot be exercised without exposed DRM/NVIDIA device access; installed iris library alone is not a GPU.'
out=Path(__file__).with_name('capabilities.json');out.write_text(json.dumps(r,indent=2)+'\n');print(out.read_text())
