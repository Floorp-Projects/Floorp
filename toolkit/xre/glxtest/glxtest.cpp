/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//////////////////////////////////////////////////////////////////////////////
//
// Explanation: See bug 639842. Safely getting GL driver info on X11 is hard,
// because the only way to do that is to create a GL context and call
// glGetString(), but with bad drivers, just creating a GL context may crash.
//
// This file implements the idea to do that in a separate process.
//
// The only non-static function here is fire_glxtest_process(). It creates a
// pipe, publishes its 'read' end as the mozilla::widget::glxtest_pipe global
// variable, forks, and runs that GLX probe in the child process, which runs the
// childgltest() static function. This creates a X connection, a GLX context,
// calls glGetString, and writes that to the 'write' end of the pipe.

#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <vector>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <gdk/gdk.h>

#if defined(MOZ_ASAN) || defined(FUZZING)
#  include <signal.h>
#endif

#ifdef __SUNPRO_CC
#  include <stdio.h>
#endif

#ifdef MOZ_X11
#  include "X11/Xlib.h"
#  include "X11/Xutil.h"
#  include <X11/extensions/Xrandr.h>
#endif

#include <vector>
#include <sys/wait.h>
#include "mozilla/ScopeExit.h"
#include "mozilla/Types.h"

#include "mozilla/GfxInfoUtils.h"

#ifdef MOZ_X11
// stuff from glx.h
typedef struct __GLXcontextRec* GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec* GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
#  define GLX_RGBA 4
#  define GLX_RED_SIZE 8
#  define GLX_GREEN_SIZE 9
#  define GLX_BLUE_SIZE 10
#  define GLX_DOUBLEBUFFER 5
#endif

// stuff from gl.h
typedef uint8_t GLubyte;
typedef uint32_t GLenum;
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02

// GLX_MESA_query_renderer
// clang-format off
#define GLX_RENDERER_VENDOR_ID_MESA                            0x8183
#define GLX_RENDERER_DEVICE_ID_MESA                            0x8184
#define GLX_RENDERER_VERSION_MESA                              0x8185
#define GLX_RENDERER_ACCELERATED_MESA                          0x8186
#define GLX_RENDERER_VIDEO_MEMORY_MESA                         0x8187
#define GLX_RENDERER_UNIFIED_MEMORY_ARCHITECTURE_MESA          0x8188
#define GLX_RENDERER_PREFERRED_PROFILE_MESA                    0x8189
#define GLX_RENDERER_OPENGL_CORE_PROFILE_VERSION_MESA          0x818A
#define GLX_RENDERER_OPENGL_COMPATIBILITY_PROFILE_VERSION_MESA 0x818B
#define GLX_RENDERER_OPENGL_ES_PROFILE_VERSION_MESA            0x818C
#define GLX_RENDERER_OPENGL_ES2_PROFILE_VERSION_MESA           0x818D
#define GLX_RENDERER_ID_MESA                                   0x818E
// clang-format on

// stuff from egl.h
typedef intptr_t EGLAttrib;
typedef int EGLBoolean;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLDeviceEXT;
typedef void* EGLDisplay;
typedef unsigned int EGLenum;
typedef int EGLint;
typedef void* EGLNativeDisplayType;
typedef void* EGLSurface;
typedef void* (*PFNEGLGETPROCADDRESS)(const char*);

#define EGL_NO_CONTEXT nullptr
#define EGL_NO_SURFACE nullptr
#define EGL_FALSE 0
#define EGL_TRUE 1
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_BLUE_SIZE 0x3022
#define EGL_GREEN_SIZE 0x3023
#define EGL_RED_SIZE 0x3024
#define EGL_NONE 0x3038
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_VENDOR 0x3053
#define EGL_EXTENSIONS 0x3055
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_OPENGL_API 0x30A2
#define EGL_DEVICE_EXT 0x322C
#define EGL_DRM_DEVICE_FILE_EXT 0x3233
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377

// stuff from xf86drm.h
#define DRM_NODE_RENDER 2
#define DRM_NODE_MAX 3

typedef struct _drmPciDeviceInfo {
  uint16_t vendor_id;
  uint16_t device_id;
  uint16_t subvendor_id;
  uint16_t subdevice_id;
  uint8_t revision_id;
} drmPciDeviceInfo, *drmPciDeviceInfoPtr;

typedef struct _drmDevice {
  char** nodes;
  int available_nodes;
  int bustype;
  union {
    void* pci;
    void* usb;
    void* platform;
    void* host1x;
  } businfo;
  union {
    drmPciDeviceInfoPtr pci;
    void* usb;
    void* platform;
    void* host1x;
  } deviceinfo;
} drmDevice, *drmDevicePtr;

// Open libGL and load needed symbols
#if defined(__OpenBSD__) || defined(__NetBSD__)
#  define LIBGL_FILENAME "libGL.so"
#  define LIBGLES_FILENAME "libGLESv2.so"
#  define LIBEGL_FILENAME "libEGL.so"
#  define LIBDRM_FILENAME "libdrm.so"
#else
#  define LIBGL_FILENAME "libGL.so.1"
#  define LIBGLES_FILENAME "libGLESv2.so.2"
#  define LIBEGL_FILENAME "libEGL.so.1"
#  define LIBDRM_FILENAME "libdrm.so.2"
#endif

#ifdef MOZ_X11
static int x_error_handler(Display*, XErrorEvent* ev) {
  record_error(
      "X error, error_code=%d, "
      "request_code=%d, minor_code=%d",
      ev->error_code, ev->request_code, ev->minor_code);
  record_flush();
  _exit(EXIT_FAILURE);
}
#endif

// childgltest is declared inside extern "C" so that the name is not mangled.
// The name is used in build/valgrind/x86_64-pc-linux-gnu.sup to suppress
// memory leak errors because we run it inside a short lived fork and we don't
// care about leaking memory
extern "C" {

#define PCI_FILL_IDENT 0x0001
#define PCI_FILL_CLASS 0x0020
#define PCI_BASE_CLASS_DISPLAY 0x03

static void get_pci_status() {
  log("GLX_TEST: get_pci_status start\n");

#if !defined(XP_FREEBSD) && !defined(XP_NETBSD) && !defined(XP_OPENBSD) && \
    !defined(XP_SOLARIS)
  if (access("/sys/bus/pci/", F_OK) != 0 &&
      access("/sys/bus/pci_express/", F_OK) != 0) {
    log("GLX_TEST: get_pci_status failed: cannot access /sys/bus/pci\n");
    return;
  }

  void* libpci = dlopen("libpci.so.3", RTLD_LAZY);
  if (!libpci) {
    libpci = dlopen("libpci.so", RTLD_LAZY);
  }
  if (!libpci) {
    record_warning("libpci missing");
    return;
  }
  auto release = mozilla::MakeScopeExit([&] { dlclose(libpci); });

  typedef struct pci_dev {
    struct pci_dev* next;
    uint16_t domain_16;
    uint8_t bus, dev, func;
    unsigned int known_fields;
    uint16_t vendor_id, device_id;
    uint16_t device_class;
  } pci_dev;

  typedef struct pci_access {
    unsigned int method;
    int writeable;
    int buscentric;
    char* id_file_name;
    int free_id_name;
    int numeric_ids;
    unsigned int id_lookup_mode;
    int debugging;
    void* error;
    void* warning;
    void* debug;
    pci_dev* devices;
  } pci_access;

  typedef pci_access* (*PCIALLOC)(void);
  PCIALLOC pci_alloc = cast<PCIALLOC>(dlsym(libpci, "pci_alloc"));

  typedef void (*PCIINIT)(pci_access*);
  PCIINIT pci_init = cast<PCIINIT>(dlsym(libpci, "pci_init"));

  typedef void (*PCICLEANUP)(pci_access*);
  PCICLEANUP pci_cleanup = cast<PCICLEANUP>(dlsym(libpci, "pci_cleanup"));

  typedef void (*PCISCANBUS)(pci_access*);
  PCISCANBUS pci_scan_bus = cast<PCISCANBUS>(dlsym(libpci, "pci_scan_bus"));

  typedef void (*PCIFILLINFO)(pci_dev*, int);
  PCIFILLINFO pci_fill_info = cast<PCIFILLINFO>(dlsym(libpci, "pci_fill_info"));

  if (!pci_alloc || !pci_cleanup || !pci_scan_bus || !pci_fill_info) {
    dlclose(libpci);
    record_warning("libpci missing methods");
    return;
  }

  pci_access* pacc = pci_alloc();
  if (!pacc) {
    record_warning("libpci alloc failed");
    return;
  }

  pci_init(pacc);
  pci_scan_bus(pacc);

  for (pci_dev* dev = pacc->devices; dev; dev = dev->next) {
    pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_CLASS);
    if (dev->device_class >> 8 == PCI_BASE_CLASS_DISPLAY && dev->vendor_id &&
        dev->device_id) {
      record_value("PCI_VENDOR_ID\n0x%04x\nPCI_DEVICE_ID\n0x%04x\n",
                   dev->vendor_id, dev->device_id);
    }
  }

  pci_cleanup(pacc);
#endif

  log("GLX_TEST: get_pci_status finished\n");
}

static void set_render_device_path(const char* render_device_path) {
  record_value("DRM_RENDERDEVICE\n%s\n", render_device_path);
}

static bool device_has_name(const drmDevice* device, const char* name) {
  for (size_t i = 0; i < DRM_NODE_MAX; i++) {
    if (!(device->available_nodes & (1 << i))) {
      continue;
    }
    if (strcmp(device->nodes[i], name) == 0) {
      return true;
    }
  }
  return false;
}

static bool get_render_name(const char* name) {
  void* libdrm = dlopen(LIBDRM_FILENAME, RTLD_LAZY);
  if (!libdrm) {
    record_warning("Failed to open libdrm");
    return false;
  }
  auto release = mozilla::MakeScopeExit([&] { dlclose(libdrm); });

  typedef int (*DRMGETDEVICES2)(uint32_t, drmDevicePtr*, int);
  DRMGETDEVICES2 drmGetDevices2 =
      cast<DRMGETDEVICES2>(dlsym(libdrm, "drmGetDevices2"));

  typedef void (*DRMFREEDEVICE)(drmDevicePtr*);
  DRMFREEDEVICE drmFreeDevice =
      cast<DRMFREEDEVICE>(dlsym(libdrm, "drmFreeDevice"));

  if (!drmGetDevices2 || !drmFreeDevice) {
    record_warning(
        "libdrm missing methods for drmGetDevices2 or drmFreeDevice");
    return false;
  }

  uint32_t flags = 0;
  int devices_len = drmGetDevices2(flags, nullptr, 0);
  if (devices_len < 0) {
    record_warning("drmGetDevices2 failed");
    return false;
  }
  drmDevice** devices = (drmDevice**)calloc(devices_len, sizeof(drmDevice*));
  if (!devices) {
    record_warning("Allocation error");
    return false;
  }
  devices_len = drmGetDevices2(flags, devices, devices_len);
  if (devices_len < 0) {
    free(devices);
    record_warning("drmGetDevices2 failed");
    return false;
  }

  const drmDevice* match = nullptr;
  for (int i = 0; i < devices_len; i++) {
    if (device_has_name(devices[i], name)) {
      match = devices[i];
      break;
    }
  }

  // Fallback path for split kms/render devices - if only one drm render node
  // exists it's most likely the one we're looking for.
  if (match && !(match->available_nodes & (1 << DRM_NODE_RENDER))) {
    match = nullptr;
    for (int i = 0; i < devices_len; i++) {
      if (devices[i]->available_nodes & (1 << DRM_NODE_RENDER)) {
        if (!match) {
          match = devices[i];
        } else {
          // more than one candidate found, stop trying.
          match = nullptr;
          break;
        }
      }
    }
    if (match) {
      record_warning(
          "DRM render node not clearly detectable. Falling back to using the "
          "only one that was found.");
    } else {
      record_warning("DRM device has no render node");
    }
  }

  bool result = false;
  if (!match) {
    record_warning("Cannot find DRM device");
  } else {
    set_render_device_path(match->nodes[DRM_NODE_RENDER]);
    record_value(
        "MESA_VENDOR_ID\n0x%04x\n"
        "MESA_DEVICE_ID\n0x%04x\n",
        match->deviceinfo.pci->vendor_id, match->deviceinfo.pci->device_id);
    result = true;
  }

  for (int i = 0; i < devices_len; i++) {
    drmFreeDevice(&devices[i]);
  }
  free(devices);
  return result;
}

static bool get_egl_gl_status(EGLDisplay dpy,
                              PFNEGLGETPROCADDRESS eglGetProcAddress) {
  typedef EGLBoolean (*PFNEGLCHOOSECONFIGPROC)(
      EGLDisplay dpy, EGLint const* attrib_list, EGLConfig* configs,
      EGLint config_size, EGLint* num_config);
  PFNEGLCHOOSECONFIGPROC eglChooseConfig =
      cast<PFNEGLCHOOSECONFIGPROC>(eglGetProcAddress("eglChooseConfig"));

  typedef EGLBoolean (*PFNEGLBINDAPIPROC)(EGLint api);
  PFNEGLBINDAPIPROC eglBindAPI =
      cast<PFNEGLBINDAPIPROC>(eglGetProcAddress("eglBindAPI"));

  typedef EGLContext (*PFNEGLCREATECONTEXTPROC)(
      EGLDisplay dpy, EGLConfig config, EGLContext share_context,
      EGLint const* attrib_list);
  PFNEGLCREATECONTEXTPROC eglCreateContext =
      cast<PFNEGLCREATECONTEXTPROC>(eglGetProcAddress("eglCreateContext"));

  typedef EGLBoolean (*PFNEGLDESTROYCONTEXTPROC)(EGLDisplay dpy,
                                                 EGLContext ctx);
  PFNEGLDESTROYCONTEXTPROC eglDestroyContext =
      cast<PFNEGLDESTROYCONTEXTPROC>(eglGetProcAddress("eglDestroyContext"));

  typedef EGLBoolean (*PFNEGLMAKECURRENTPROC)(
      EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext context);
  PFNEGLMAKECURRENTPROC eglMakeCurrent =
      cast<PFNEGLMAKECURRENTPROC>(eglGetProcAddress("eglMakeCurrent"));

  typedef const char* (*PFNEGLQUERYDEVICESTRINGEXTPROC)(EGLDeviceEXT device,
                                                        EGLint name);
  PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT =
      cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(
          eglGetProcAddress("eglQueryDeviceStringEXT"));

  typedef EGLBoolean (*PFNEGLQUERYDISPLAYATTRIBEXTPROC)(
      EGLDisplay dpy, EGLint name, EGLAttrib* value);
  PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT =
      cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
          eglGetProcAddress("eglQueryDisplayAttribEXT"));

  log("GLX_TEST: get_egl_gl_status start\n");

  if (!eglChooseConfig || !eglCreateContext || !eglDestroyContext ||
      !eglMakeCurrent || !eglQueryDeviceStringEXT) {
    record_warning("libEGL missing methods for GL test");
    return false;
  }

  typedef GLubyte* (*PFNGLGETSTRING)(GLenum);
  PFNGLGETSTRING glGetString =
      cast<PFNGLGETSTRING>(eglGetProcAddress("glGetString"));

#if defined(__arm__) || defined(__aarch64__)
  bool useGles = true;
#else
  bool useGles = false;
#endif

  std::vector<EGLint> attribs;
  attribs.push_back(EGL_RED_SIZE);
  attribs.push_back(8);
  attribs.push_back(EGL_GREEN_SIZE);
  attribs.push_back(8);
  attribs.push_back(EGL_BLUE_SIZE);
  attribs.push_back(8);
  if (useGles) {
    attribs.push_back(EGL_RENDERABLE_TYPE);
    attribs.push_back(EGL_OPENGL_ES2_BIT);
  }
  attribs.push_back(EGL_NONE);

  EGLConfig config;
  EGLint num_config;
  if (eglChooseConfig(dpy, attribs.data(), &config, 1, &num_config) ==
      EGL_FALSE) {
    record_warning("eglChooseConfig returned an error");
    return false;
  }

  EGLenum api = useGles ? EGL_OPENGL_ES_API : EGL_OPENGL_API;
  if (eglBindAPI(api) == EGL_FALSE) {
    record_warning("eglBindAPI returned an error");
    return false;
  }

  EGLint ctx_attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
  EGLContext ectx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctx_attrs);
  if (!ectx) {
    EGLint ctx_attrs_fallback[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
    ectx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, ctx_attrs_fallback);
    if (!ectx) {
      record_warning("eglCreateContext returned an error");
      return false;
    }
  }

  if (eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ectx) == EGL_FALSE) {
    eglDestroyContext(dpy, ectx);
    record_warning("eglMakeCurrent returned an error");
    return false;
  }
  eglDestroyContext(dpy, ectx);

  // Implementations disagree about whether eglGetProcAddress or dlsym
  // should be used for getting functions from the actual API, see
  // https://github.com/anholt/libepoxy/commit/14f24485e33816139398d1bd170d617703473738
  void* libgl = nullptr;
  if (!glGetString) {
    libgl = dlopen(LIBGL_FILENAME, RTLD_LAZY);
    if (!libgl) {
      libgl = dlopen(LIBGLES_FILENAME, RTLD_LAZY);
      if (!libgl) {
        record_warning(LIBGL_FILENAME " and " LIBGLES_FILENAME " missing");
        return false;
      }
    }

    glGetString = cast<PFNGLGETSTRING>(dlsym(libgl, "glGetString"));
    if (!glGetString) {
      dlclose(libgl);
      record_warning("libEGL, libGL and libGLESv2 are missing glGetString");
      return false;
    }
  }
  auto release = mozilla::MakeScopeExit([&] {
    if (libgl) {
      dlclose(libgl);
    }
  });

  const GLubyte* versionString = glGetString(GL_VERSION);
  const GLubyte* vendorString = glGetString(GL_VENDOR);
  const GLubyte* rendererString = glGetString(GL_RENDERER);

  if (versionString && vendorString && rendererString) {
    record_value("VENDOR\n%s\nRENDERER\n%s\nVERSION\n%s\nTFP\nTRUE\n",
                 vendorString, rendererString, versionString);
  } else {
    record_warning("EGL glGetString returned null");
    return false;
  }

  EGLDeviceEXT device;
  if (eglQueryDisplayAttribEXT(dpy, EGL_DEVICE_EXT, (EGLAttrib*)&device) ==
      EGL_TRUE) {
    const char* deviceExtensions =
        eglQueryDeviceStringEXT(device, EGL_EXTENSIONS);
    if (deviceExtensions &&
        strstr(deviceExtensions, "EGL_MESA_device_software")) {
      record_value("MESA_ACCELERATED\nFALSE\n");
    } else {
      const char* deviceString =
          eglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT);
      if (!deviceString || !get_render_name(deviceString)) {
        const char* renderNodeString =
            eglQueryDeviceStringEXT(device, EGL_DRM_RENDER_NODE_FILE_EXT);
        if (renderNodeString) {
          set_render_device_path(renderNodeString);
        }
      }
    }
  }

  log("GLX_TEST: get_egl_gl_status finished\n");
  return true;
}

static bool get_egl_status(EGLNativeDisplayType native_dpy) {
  log("GLX_TEST: get_egl_status start\n");

  EGLDisplay dpy = nullptr;

  typedef EGLBoolean (*PFNEGLTERMINATEPROC)(EGLDisplay dpy);
  PFNEGLTERMINATEPROC eglTerminate = nullptr;

  void* libegl = dlopen(LIBEGL_FILENAME, RTLD_LAZY);
  if (!libegl) {
    record_warning("libEGL missing");
    return false;
  }
  auto release = mozilla::MakeScopeExit([&] {
    if (dpy) {
      eglTerminate(dpy);
    }
    // Unload libegl here causes ASAN/MemLeaks failures on Linux
    // as libegl may not be meant to be unloaded runtime.
    // See 1304156 for reference.
    // dlclose(libegl);
  });

  PFNEGLGETPROCADDRESS eglGetProcAddress =
      cast<PFNEGLGETPROCADDRESS>(dlsym(libegl, "eglGetProcAddress"));

  if (!eglGetProcAddress) {
    record_warning("no eglGetProcAddress");
    return false;
  }

  typedef EGLDisplay (*PFNEGLGETDISPLAYPROC)(void* native_display);
  PFNEGLGETDISPLAYPROC eglGetDisplay =
      cast<PFNEGLGETDISPLAYPROC>(eglGetProcAddress("eglGetDisplay"));

  typedef EGLBoolean (*PFNEGLINITIALIZEPROC)(EGLDisplay dpy, EGLint* major,
                                             EGLint* minor);
  PFNEGLINITIALIZEPROC eglInitialize =
      cast<PFNEGLINITIALIZEPROC>(eglGetProcAddress("eglInitialize"));
  eglTerminate = cast<PFNEGLTERMINATEPROC>(eglGetProcAddress("eglTerminate"));

  if (!eglGetDisplay || !eglInitialize || !eglTerminate) {
    record_warning("libEGL missing methods");
    return false;
  }

  dpy = eglGetDisplay(native_dpy);
  if (!dpy) {
    record_warning("libEGL no display");
    return false;
  }

  EGLint major, minor;
  if (!eglInitialize(dpy, &major, &minor)) {
    record_warning("libEGL initialize failed");
    return false;
  }

  typedef const char* (*PFNEGLGETDISPLAYDRIVERNAMEPROC)(EGLDisplay dpy);
  PFNEGLGETDISPLAYDRIVERNAMEPROC eglGetDisplayDriverName =
      cast<PFNEGLGETDISPLAYDRIVERNAMEPROC>(
          eglGetProcAddress("eglGetDisplayDriverName"));
  if (eglGetDisplayDriverName) {
    const char* driDriver = eglGetDisplayDriverName(dpy);
    if (driDriver) {
      record_value("DRI_DRIVER\n%s\n", driDriver);
    }
  }

  bool ret = get_egl_gl_status(dpy, eglGetProcAddress);
  log("GLX_TEST: get_egl_status finished with return: %d\n", ret);

  return ret;
}

#ifdef MOZ_X11
static void get_xrandr_info(Display* dpy) {
  log("GLX_TEST: get_xrandr_info start\n");

  // When running on remote X11 the xrandr version may be stuck on an ancient
  // version. There are still setups using remote X11 out there, so make sure we
  // don't crash.
  int eventBase, errorBase, major, minor;
  if (!XRRQueryExtension(dpy, &eventBase, &errorBase) ||
      !XRRQueryVersion(dpy, &major, &minor) ||
      !(major > 1 || (major == 1 && minor >= 4))) {
    log("GLX_TEST: get_xrandr_info failed, old version.\n");
    return;
  }

  Window root = RootWindow(dpy, DefaultScreen(dpy));
  XRRProviderResources* pr = XRRGetProviderResources(dpy, root);
  if (!pr) {
    log("GLX_TEST: XRRGetProviderResources failed.\n");
    return;
  }
  XRRScreenResources* res = XRRGetScreenResourcesCurrent(dpy, root);
  if (!res) {
    XRRFreeProviderResources(pr);
    log("GLX_TEST: XRRGetScreenResourcesCurrent failed.\n");
    return;
  }
  if (pr->nproviders != 0) {
    record_value("DDX_DRIVER\n");
    for (int i = 0; i < pr->nproviders; i++) {
      XRRProviderInfo* info = XRRGetProviderInfo(dpy, res, pr->providers[i]);
      if (info) {
        record_value("%s%s", info->name, i == pr->nproviders - 1 ? ";\n" : ";");
        XRRFreeProviderInfo(info);
      }
    }
  }
  XRRFreeScreenResources(res);
  XRRFreeProviderResources(pr);

  log("GLX_TEST: get_xrandr_info finished\n");
}

void glxtest() {
  log("GLX_TEST: glxtest start\n");

  Display* dpy = nullptr;
  void* libgl = dlopen(LIBGL_FILENAME, RTLD_LAZY);
  if (!libgl) {
    record_error(LIBGL_FILENAME " missing");
    return;
  }
  auto release = mozilla::MakeScopeExit([&] {
    if (dpy) {
#  ifdef MOZ_ASAN
      XCloseDisplay(dpy);
#  else
      // This XSync call wanted to be instead:
      //   XCloseDisplay(dpy);
      // but this can cause 1-minute stalls on certain setups using Nouveau, see
      // bug 973192
      XSync(dpy, False);
#  endif
    }
    dlclose(libgl);
  });

  typedef void* (*PFNGLXGETPROCADDRESS)(const char*);
  PFNGLXGETPROCADDRESS glXGetProcAddress =
      cast<PFNGLXGETPROCADDRESS>(dlsym(libgl, "glXGetProcAddress"));

  if (!glXGetProcAddress) {
    record_error("no glXGetProcAddress");
    return;
  }

  typedef GLXFBConfig* (*PFNGLXQUERYEXTENSION)(Display*, int*, int*);
  PFNGLXQUERYEXTENSION glXQueryExtension =
      cast<PFNGLXQUERYEXTENSION>(glXGetProcAddress("glXQueryExtension"));

  typedef GLXFBConfig* (*PFNGLXQUERYVERSION)(Display*, int*, int*);
  PFNGLXQUERYVERSION glXQueryVersion =
      cast<PFNGLXQUERYVERSION>(dlsym(libgl, "glXQueryVersion"));

  typedef XVisualInfo* (*PFNGLXCHOOSEVISUAL)(Display*, int, int*);
  PFNGLXCHOOSEVISUAL glXChooseVisual =
      cast<PFNGLXCHOOSEVISUAL>(glXGetProcAddress("glXChooseVisual"));

  typedef GLXContext (*PFNGLXCREATECONTEXT)(Display*, XVisualInfo*, GLXContext,
                                            Bool);
  PFNGLXCREATECONTEXT glXCreateContext =
      cast<PFNGLXCREATECONTEXT>(glXGetProcAddress("glXCreateContext"));

  typedef Bool (*PFNGLXMAKECURRENT)(Display*, GLXDrawable, GLXContext);
  PFNGLXMAKECURRENT glXMakeCurrent =
      cast<PFNGLXMAKECURRENT>(glXGetProcAddress("glXMakeCurrent"));

  typedef void (*PFNGLXDESTROYCONTEXT)(Display*, GLXContext);
  PFNGLXDESTROYCONTEXT glXDestroyContext =
      cast<PFNGLXDESTROYCONTEXT>(glXGetProcAddress("glXDestroyContext"));

  typedef GLubyte* (*PFNGLGETSTRING)(GLenum);
  PFNGLGETSTRING glGetString =
      cast<PFNGLGETSTRING>(glXGetProcAddress("glGetString"));

  if (!glXQueryExtension || !glXQueryVersion || !glXChooseVisual ||
      !glXCreateContext || !glXMakeCurrent || !glXDestroyContext ||
      !glGetString) {
    record_error(LIBGL_FILENAME " missing methods");
    return;
  }

  ///// Open a connection to the X server /////
  dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    record_error("Unable to open a connection to the X server");
    return;
  }

  ///// Check that the GLX extension is present /////
  if (!glXQueryExtension(dpy, nullptr, nullptr)) {
    record_error("GLX extension missing");
    return;
  }

  XSetErrorHandler(x_error_handler);

  ///// Get a visual /////
  int attribs[] = {GLX_RGBA, GLX_RED_SIZE,  1, GLX_GREEN_SIZE,
                   1,        GLX_BLUE_SIZE, 1, None};
  XVisualInfo* vInfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
  if (!vInfo) {
    int attribs2[] = {GLX_RGBA, GLX_RED_SIZE,  1, GLX_GREEN_SIZE,
                      1,        GLX_BLUE_SIZE, 1, GLX_DOUBLEBUFFER,
                      None};
    vInfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs2);
    if (!vInfo) {
      record_error("No visuals found");
      return;
    }
  }

  // using a X11 Window instead of a GLXPixmap does not crash
  // fglrx in indirect rendering. bug 680644
  Window window;
  XSetWindowAttributes swa;
  swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vInfo->screen),
                                 vInfo->visual, AllocNone);

  swa.border_pixel = 0;
  window = XCreateWindow(dpy, RootWindow(dpy, vInfo->screen), 0, 0, 16, 16, 0,
                         vInfo->depth, InputOutput, vInfo->visual,
                         CWBorderPixel | CWColormap, &swa);

  ///// Get a GL context and make it current //////
  GLXContext context = glXCreateContext(dpy, vInfo, nullptr, True);
  glXMakeCurrent(dpy, window, context);

  ///// Look for this symbol to determine texture_from_pixmap support /////
  void* glXBindTexImageEXT = glXGetProcAddress("glXBindTexImageEXT");

  ///// Get GL vendor/renderer/versions strings /////
  const GLubyte* versionString = glGetString(GL_VERSION);
  const GLubyte* vendorString = glGetString(GL_VENDOR);
  const GLubyte* rendererString = glGetString(GL_RENDERER);

  if (versionString && vendorString && rendererString) {
    record_value("VENDOR\n%s\nRENDERER\n%s\nVERSION\n%s\nTFP\n%s\n",
                 vendorString, rendererString, versionString,
                 glXBindTexImageEXT ? "TRUE" : "FALSE");
  } else {
    record_error("glGetString returned null");
  }

  // If GLX_MESA_query_renderer is available, populate additional data.
  typedef Bool (*PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC)(
      int attribute, unsigned int* value);
  PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC
  glXQueryCurrentRendererIntegerMESAProc =
      cast<PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC>(
          glXGetProcAddress("glXQueryCurrentRendererIntegerMESA"));
  if (glXQueryCurrentRendererIntegerMESAProc) {
    unsigned int vendorId, deviceId, accelerated, videoMemoryMB;
    glXQueryCurrentRendererIntegerMESAProc(GLX_RENDERER_VENDOR_ID_MESA,
                                           &vendorId);
    glXQueryCurrentRendererIntegerMESAProc(GLX_RENDERER_DEVICE_ID_MESA,
                                           &deviceId);
    glXQueryCurrentRendererIntegerMESAProc(GLX_RENDERER_ACCELERATED_MESA,
                                           &accelerated);
    glXQueryCurrentRendererIntegerMESAProc(GLX_RENDERER_VIDEO_MEMORY_MESA,
                                           &videoMemoryMB);

    // Truncate IDs to 4 digits- that's all PCI IDs are.
    vendorId &= 0xFFFF;
    deviceId &= 0xFFFF;

    record_value(
        "MESA_VENDOR_ID\n0x%04x\n"
        "MESA_DEVICE_ID\n0x%04x\n"
        "MESA_ACCELERATED\n%s\n"
        "MESA_VRAM\n%dMB\n",
        vendorId, deviceId, accelerated ? "TRUE" : "FALSE", videoMemoryMB);
  }

  // From Mesa's GL/internal/dri_interface.h, to be used by DRI clients.
  typedef const char* (*PFNGLXGETSCREENDRIVERPROC)(Display* dpy, int scrNum);
  PFNGLXGETSCREENDRIVERPROC glXGetScreenDriverProc =
      cast<PFNGLXGETSCREENDRIVERPROC>(glXGetProcAddress("glXGetScreenDriver"));
  if (glXGetScreenDriverProc) {
    const char* driDriver = glXGetScreenDriverProc(dpy, DefaultScreen(dpy));
    if (driDriver) {
      record_value("DRI_DRIVER\n%s\n", driDriver);
    }
  }

  // Get monitor and DDX driver information
  get_xrandr_info(dpy);

  ///// Clean up. Indeed, the parent process might fail to kill us (e.g. if it
  ///// doesn't need to check GL info) so we might be staying alive for longer
  ///// than expected, so it's important to consume as little memory as
  ///// possible. Also we want to check that we're able to do that too without
  ///// generating X errors.
  glXMakeCurrent(dpy, None,
                 nullptr);  // must release the GL context before destroying it
  glXDestroyContext(dpy, context);
  XDestroyWindow(dpy, window);
  XFreeColormap(dpy, swa.colormap);
  XFree(vInfo);

  record_value("TEST_TYPE\nGLX\n");
  log("GLX_TEST: glxtest finished\n");
}

bool x11_egltest() {
  log("GLX_TEST: x11_egltest start\n");

  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    log("GLX_TEST: XOpenDisplay failed.\n");
    return false;
  }
#  ifdef MOZ_ASAN
  auto release = mozilla::MakeScopeExit([&] {
    // Bug 1715245: Closing the display connection here crashes on NV prop.
    // drivers. Just leave it open, the process will exit shortly after anyway.
    XCloseDisplay(dpy);
  });
#  endif

  XSetErrorHandler(x_error_handler);

  if (!get_egl_status(dpy)) {
    return false;
  }

  // Bug 1667621: 30bit "Deep Color" is broken on EGL on Mesa (as of 2021/10).
  // Disable all non-standard depths for the initial EGL roleout.
  int screenCount = ScreenCount(dpy);
  for (int idx = 0; idx < screenCount; idx++) {
    int depth = DefaultDepth(dpy, idx);
    if (depth != 24) {
      log("GLX_TEST: DefaultDepth() is %d, expected to be 24. See Bug "
          "1667621.\n",
          depth);
      return false;
    }
  }

  // Get monitor and DDX driver information
  get_xrandr_info(dpy);

  record_value("TEST_TYPE\nEGL\n");

  log("GLX_TEST: x11_egltest finished\n");
  return true;
}
#endif

#ifdef MOZ_WAYLAND
void wayland_egltest() {
  log("GLX_TEST: wayland_egltest start\n");

  static auto sWlDisplayConnect = (struct wl_display * (*)(const char*))
      dlsym(RTLD_DEFAULT, "wl_display_connect");
  static auto sWlDisplayRoundtrip =
      (int (*)(struct wl_display*))dlsym(RTLD_DEFAULT, "wl_display_roundtrip");
  static auto sWlDisplayDisconnect = (void (*)(struct wl_display*))dlsym(
      RTLD_DEFAULT, "wl_display_disconnect");

  if (!sWlDisplayConnect || !sWlDisplayRoundtrip || !sWlDisplayDisconnect) {
    record_error("Missing Wayland libraries");
    return;
  }

  // NOTE: returns false to fall back to X11 when the Wayland socket doesn't
  // exist but fails with record_error if something actually went wrong
  struct wl_display* dpy = sWlDisplayConnect(nullptr);
  if (!dpy) {
    record_error("Could not connect to wayland display, WAYLAND_DISPLAY=%s",
                 getenv("WAYLAND_DISPLAY"));
    return;
  }

  if (!get_egl_status((EGLNativeDisplayType)dpy)) {
    record_error("EGL test failed");
  }

  // This is enough to crash some broken NVIDIA prime + Wayland setups, see
  // https://github.com/NVIDIA/egl-wayland/issues/41 and bug 1768260.
  sWlDisplayRoundtrip(dpy);

  sWlDisplayDisconnect(dpy);
  record_value("TEST_TYPE\nEGL\n");
  log("GLX_TEST: wayland_egltest finished\n");
}
#endif

int childgltest(bool aWayland) {
  log("GLX_TEST: childgltest start\n");

  // Get a list of all GPUs from the PCI bus.
  get_pci_status();

#ifdef MOZ_WAYLAND
  if (aWayland) {
    wayland_egltest();
  }
#endif
#ifdef MOZ_X11
  if (!aWayland) {
    // TODO: --display command line argument is not properly handled
    if (!x11_egltest()) {
      glxtest();
    }
  }
#endif
  // Finally write buffered data to the pipe.
  record_flush();

  log("GLX_TEST: childgltest finished\n");
  return EXIT_SUCCESS;
}

}  // extern "C"

static void PrintUsage() {
  printf(
      "Firefox OpenGL probe utility\n"
      "\n"
      "usage: glxtest [options]\n"
      "\n"
      "Options:\n"
      "\n"
      "  -h --help                 show this message\n"
      "  -f --fd num               where to print output, default it stdout\n"
      "  -w --wayland              probe OpenGL/EGL on Wayland (default is "
      "X11)\n"
      "\n");
}

int main(int argc, char** argv) {
  struct option longOptions[] = {{"help", no_argument, nullptr, 'h'},
                                 {"fd", required_argument, nullptr, 'f'},
                                 {"wayland", no_argument, nullptr, 'w'},
                                 {nullptr, 0, nullptr, 0}};
  const char* shortOptions = "hf:w";
  int c;
  bool wayland = false;
  while ((c = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) !=
         -1) {
    switch (c) {
      case 'w':
        wayland = true;
        break;
      case 'f':
        output_pipe = atoi(optarg);
        break;
      case 'h':
#ifdef MOZ_WAYLAND
        // Dummy call to mozgtk to prevent the linker from removing
        // the dependency with --as-needed.
        // see toolkit/library/moz.build for details.
        gdk_display_get_default();
#endif
        PrintUsage();
        return 0;
      default:
        break;
    }
  }
  if (getenv("MOZ_AVOID_OPENGL_ALTOGETHER")) {
    const char* msg = "ERROR\nMOZ_AVOID_OPENGL_ALTOGETHER envvar set";
    MOZ_UNUSED(write(output_pipe, msg, strlen(msg)));
    exit(EXIT_FAILURE);
  }
  const char* env = getenv("MOZ_GFX_DEBUG");
  enable_logging = env && *env == '1';
  if (!enable_logging) {
    close_logging();
  }
#if defined(MOZ_ASAN) || defined(FUZZING)
  // If handle_segv=1 (default), then glxtest crash will print a sanitizer
  // report which can confuse the harness in fuzzing automation.
  signal(SIGSEGV, SIG_DFL);
#endif
  return childgltest(wayland);
}
