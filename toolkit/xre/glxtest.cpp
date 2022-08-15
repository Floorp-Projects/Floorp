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

#include "mozilla/Unused.h"
#include "nsAppRunner.h"  // for IsWaylandEnabled on IsX11EGLEnabled
#include "stdint.h"

#ifdef __SUNPRO_CC
#  include <stdio.h>
#endif

#ifdef MOZ_X11
#  include "X11/Xlib.h"
#  include "X11/Xutil.h"
#  include <X11/extensions/Xrandr.h>
#endif

#ifdef MOZ_WAYLAND
#  include "mozilla/widget/mozwayland.h"
#  include "mozilla/widget/xdg-output-unstable-v1-client-protocol.h"
#  include "prlink.h"
#  include "va/va.h"
#endif

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

#define EXIT_FAILURE_BUFFER_TOO_SMALL 2

namespace mozilla {
namespace widget {
// the read end of the pipe, which will be used by GfxInfo
extern int glxtest_pipe;
// the PID of the glxtest process, to pass to waitpid()
extern pid_t glxtest_pid;
}  // namespace widget
}  // namespace mozilla

// the write end of the pipe, which we're going to write to
static int write_end_of_the_pipe = -1;

// our buffer, size and used length
static char* glxtest_buf = nullptr;
static int glxtest_bufsize = 0;
static int glxtest_length = 0;

static char* glxtest_render_device_path = nullptr;

// C++ standard collides with C standard in that it doesn't allow casting void*
// to function pointer types. So the work-around is to convert first to size_t.
// http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
template <typename func_ptr_type>
static func_ptr_type cast(void* ptr) {
  return reinterpret_cast<func_ptr_type>(reinterpret_cast<size_t>(ptr));
}

static void record_value(const char* format, ...) {
  // Don't add more if the buffer is full.
  if (glxtest_bufsize <= glxtest_length) {
    return;
  }

  // Append the new values to the buffer, not to exceed the remaining space.
  int remaining = glxtest_bufsize - glxtest_length;
  va_list args;
  va_start(args, format);
  int max_added =
      vsnprintf(glxtest_buf + glxtest_length, remaining, format, args);
  va_end(args);

  // snprintf returns how many char it could have added, not how many it added.
  // It is important to get this right since it will control how many chars we
  // will attempt to write to the pipe fd.
  if (max_added > remaining) {
    glxtest_length += remaining;
  } else {
    glxtest_length += max_added;
  }
}

static void record_error(const char* str) { record_value("ERROR\n%s\n", str); }

static void record_warning(const char* str) {
  record_value("WARNING\n%s\n", str);
}

static void record_flush() {
  mozilla::Unused << write(write_end_of_the_pipe, glxtest_buf, glxtest_length);
}

#ifdef MOZ_X11
static int x_error_handler(Display*, XErrorEvent* ev) {
  record_value(
      "ERROR\nX error, error_code=%d, "
      "request_code=%d, minor_code=%d\n",
      ev->error_code, ev->request_code, ev->minor_code);
  record_flush();
  _exit(EXIT_FAILURE);
  return 0;
}
#endif

// childgltest is declared inside extern "C" so that the name is not mangled.
// The name is used in build/valgrind/x86_64-pc-linux-gnu.sup to suppress
// memory leak errors because we run it inside a short lived fork and we don't
// care about leaking memory
extern "C" {

static void close_logging() {
  // we want to redirect to /dev/null stdout, stderr, and while we're at it,
  // any PR logging file descriptors. To that effect, we redirect all positive
  // file descriptors up to what open() returns here. In particular, 1 is stdout
  // and 2 is stderr.
  int fd = open("/dev/null", O_WRONLY);
  for (int i = 1; i < fd; i++) {
    dup2(fd, i);
  }
  close(fd);

  if (getenv("MOZ_AVOID_OPENGL_ALTOGETHER")) {
    const char* msg = "ERROR\nMOZ_AVOID_OPENGL_ALTOGETHER envvar set";
    mozilla::Unused << write(write_end_of_the_pipe, msg, strlen(msg));
    exit(EXIT_FAILURE);
  }
}

#define PCI_FILL_IDENT 0x0001
#define PCI_FILL_CLASS 0x0020
#define PCI_BASE_CLASS_DISPLAY 0x03

static int get_pci_status() {
  if (access("/sys/bus/pci/", F_OK) != 0 &&
      access("/sys/bus/pci_express/", F_OK) != 0) {
    record_warning("cannot access /sys/bus/pci");
    return 0;
  }

  void* libpci = dlopen("libpci.so.3", RTLD_LAZY);
  if (!libpci) {
    libpci = dlopen("libpci.so", RTLD_LAZY);
  }
  if (!libpci) {
    record_warning("libpci missing");
    return 0;
  }

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
    return 0;
  }

  pci_access* pacc = pci_alloc();
  if (!pacc) {
    dlclose(libpci);
    record_warning("libpci alloc failed");
    return 0;
  }

  pci_init(pacc);
  pci_scan_bus(pacc);

  int count = 0;
  for (pci_dev* dev = pacc->devices; dev; dev = dev->next) {
    pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_CLASS);
    if (dev->device_class >> 8 == PCI_BASE_CLASS_DISPLAY && dev->vendor_id &&
        dev->device_id) {
      ++count;
      record_value("PCI_VENDOR_ID\n0x%04x\nPCI_DEVICE_ID\n0x%04x\n",
                   dev->vendor_id, dev->device_id);
    }
  }

  pci_cleanup(pacc);
  dlclose(libpci);
  return count;
}

#ifdef MOZ_WAYLAND
static void set_render_device_path(const char* render_device_path) {
  record_value("DRM_RENDERDEVICE\n%s\n", render_device_path);
  glxtest_render_device_path = strdup(render_device_path);
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

  typedef int (*DRMGETDEVICES2)(uint32_t, drmDevicePtr*, int);
  DRMGETDEVICES2 drmGetDevices2 =
      cast<DRMGETDEVICES2>(dlsym(libdrm, "drmGetDevices2"));

  typedef void (*DRMFREEDEVICE)(drmDevicePtr*);
  DRMFREEDEVICE drmFreeDevice =
      cast<DRMFREEDEVICE>(dlsym(libdrm, "drmFreeDevice"));

  if (!drmGetDevices2 || !drmFreeDevice) {
    record_warning(
        "libdrm missing methods for drmGetDevices2 or drmFreeDevice");
    dlclose(libdrm);
    return false;
  }

  uint32_t flags = 0;
  int devices_len = drmGetDevices2(flags, nullptr, 0);
  if (devices_len < 0) {
    record_warning("drmGetDevices2 failed");
    dlclose(libdrm);
    return false;
  }
  drmDevice** devices = (drmDevice**)calloc(devices_len, sizeof(drmDevice*));
  if (!devices) {
    record_warning("Allocation error");
    dlclose(libdrm);
    return false;
  }
  devices_len = drmGetDevices2(flags, devices, devices_len);
  if (devices_len < 0) {
    free(devices);
    record_warning("drmGetDevices2 failed");
    dlclose(libdrm);
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

  dlclose(libdrm);
  return result;
}
#endif

static bool get_gles_status(EGLDisplay dpy,
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
      EGLDisplay dpy, EGLint name, EGLAttrib * value);
  PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT =
      cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
          eglGetProcAddress("eglQueryDisplayAttribEXT"));

  if (!eglChooseConfig || !eglCreateContext || !eglMakeCurrent ||
      !eglQueryDeviceStringEXT) {
    record_warning("libEGL missing methods for GL test");
    return false;
  }

  typedef GLubyte* (*PFNGLGETSTRING)(GLenum);
  PFNGLGETSTRING glGetString =
      cast<PFNGLGETSTRING>(eglGetProcAddress("glGetString"));

#if defined(__aarch64__)
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
    record_warning("eglMakeCurrent returned an error");
    return false;
  }

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

  const GLubyte* versionString = glGetString(GL_VERSION);
  const GLubyte* vendorString = glGetString(GL_VENDOR);
  const GLubyte* rendererString = glGetString(GL_RENDERER);

  if (versionString && vendorString && rendererString) {
    record_value("VENDOR\n%s\nRENDERER\n%s\nVERSION\n%s\nTFP\nTRUE\n",
                 vendorString, rendererString, versionString);
  } else {
    record_warning("EGL glGetString returned null");
    if (libgl) {
      dlclose(libgl);
    }
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
#ifdef MOZ_WAYLAND
      const char* deviceString =
          eglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT);
      if (!deviceString || !get_render_name(deviceString)) {
        const char* renderNodeString =
            eglQueryDeviceStringEXT(device, EGL_DRM_RENDER_NODE_FILE_EXT);
        if (renderNodeString) {
          set_render_device_path(renderNodeString);
        }
      }
#endif
    }
  }

  if (libgl) {
    dlclose(libgl);
  }
  return true;
}

static bool get_egl_status(EGLNativeDisplayType native_dpy, bool gles_test,
                           bool require_driver) {
  void* libegl = dlopen(LIBEGL_FILENAME, RTLD_LAZY);
  if (!libegl) {
    record_warning("libEGL missing");
    return false;
  }

  PFNEGLGETPROCADDRESS eglGetProcAddress =
      cast<PFNEGLGETPROCADDRESS>(dlsym(libegl, "eglGetProcAddress"));

  if (!eglGetProcAddress) {
    dlclose(libegl);
    record_warning("no eglGetProcAddress");
    return false;
  }

  typedef EGLDisplay (*PFNEGLGETDISPLAYPROC)(void* native_display);
  PFNEGLGETDISPLAYPROC eglGetDisplay =
      cast<PFNEGLGETDISPLAYPROC>(eglGetProcAddress("eglGetDisplay"));

  typedef EGLBoolean (*PFNEGLINITIALIZEPROC)(EGLDisplay dpy, EGLint * major,
                                             EGLint * minor);
  PFNEGLINITIALIZEPROC eglInitialize =
      cast<PFNEGLINITIALIZEPROC>(eglGetProcAddress("eglInitialize"));

  typedef EGLBoolean (*PFNEGLTERMINATEPROC)(EGLDisplay dpy);
  PFNEGLTERMINATEPROC eglTerminate =
      cast<PFNEGLTERMINATEPROC>(eglGetProcAddress("eglTerminate"));

  if (!eglGetDisplay || !eglInitialize || !eglTerminate) {
    dlclose(libegl);
    record_warning("libEGL missing methods");
    return false;
  }

  EGLDisplay dpy = eglGetDisplay(native_dpy);
  if (!dpy) {
    dlclose(libegl);
    record_warning("libEGL no display");
    return false;
  }

  EGLint major, minor;
  if (!eglInitialize(dpy, &major, &minor)) {
    dlclose(libegl);
    record_warning("libEGL initialize failed");
    return false;
  }

  typedef const char* (*PFNEGLGETDISPLAYDRIVERNAMEPROC)(EGLDisplay dpy);
  PFNEGLGETDISPLAYDRIVERNAMEPROC eglGetDisplayDriverName =
      cast<PFNEGLGETDISPLAYDRIVERNAMEPROC>(
          eglGetProcAddress("eglGetDisplayDriverName"));
  if (eglGetDisplayDriverName) {
    // TODO(aosmond): If the driver name is empty, we probably aren't using Mesa
    // and instead a proprietary GL, most likely NVIDIA's. The PCI device list
    // in combination with the vendor name is very likely sufficient to identify
    // the device.
    const char* driDriver = eglGetDisplayDriverName(dpy);
    if (driDriver) {
      record_value("DRI_DRIVER\n%s\n", driDriver);
    }
  } else if (require_driver) {
    record_warning("libEGL missing eglGetDisplayDriverName");
    eglTerminate(dpy);
    dlclose(libegl);
    return false;
  }

  if (gles_test && !get_gles_status(dpy, eglGetProcAddress)) {
    eglTerminate(dpy);
    dlclose(libegl);
    return false;
  }

  eglTerminate(dpy);
  dlclose(libegl);
  return true;
}

#ifdef MOZ_X11
static void get_xrandr_info(Display* dpy) {
  // When running on remote X11 the xrandr version may be stuck on an ancient
  // version. There are still setups using remote X11 out there, so make sure we
  // don't crash.
  int eventBase, errorBase, major, minor;
  if (!XRRQueryExtension(dpy, &eventBase, &errorBase) ||
      !XRRQueryVersion(dpy, &major, &minor) ||
      !(major > 1 || (major == 1 && minor >= 4))) {
    return;
  }

  Window root = RootWindow(dpy, DefaultScreen(dpy));
  XRRProviderResources* pr = XRRGetProviderResources(dpy, root);
  XRRScreenResources* res = XRRGetScreenResourcesCurrent(dpy, root);
  if (pr->nproviders != 0) {
    record_value("DDX_DRIVER\n");
    for (int i = 0; i < pr->nproviders; i++) {
      XRRProviderInfo* info = XRRGetProviderInfo(dpy, res, pr->providers[i]);
      record_value("%s%s", info->name, i == pr->nproviders - 1 ? ";\n" : ";");
    }
  }
}

static void get_glx_status(int* gotGlxInfo, int* gotDriDriver) {
  void* libgl = dlopen(LIBGL_FILENAME, RTLD_LAZY);
  if (!libgl) {
    record_error(LIBGL_FILENAME " missing");
    return;
  }

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
  Display* dpy = XOpenDisplay(nullptr);
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
    *gotGlxInfo = 1;
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
  typedef const char* (*PFNGLXGETSCREENDRIVERPROC)(Display * dpy, int scrNum);
  PFNGLXGETSCREENDRIVERPROC glXGetScreenDriverProc =
      cast<PFNGLXGETSCREENDRIVERPROC>(glXGetProcAddress("glXGetScreenDriver"));
  if (glXGetScreenDriverProc) {
    const char* driDriver = glXGetScreenDriverProc(dpy, DefaultScreen(dpy));
    if (driDriver) {
      *gotDriDriver = 1;
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

#  ifdef NS_FREE_PERMANENT_DATA  // conditionally defined in nscore.h, don't
                                 // forget to #include it above
  XCloseDisplay(dpy);
#  else
  // This XSync call wanted to be instead:
  //   XCloseDisplay(dpy);
  // but this can cause 1-minute stalls on certain setups using Nouveau, see bug
  // 973192
  XSync(dpy, False);
#  endif

  dlclose(libgl);
}

static bool x11_egltest(int pci_count) {
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    return false;
  }
  XSetErrorHandler(x_error_handler);

  // Bug 1667621: 30bit "Deep Color" is broken on EGL on Mesa (as of 2021/10).
  // Disable all non-standard depths for the initial EGL roleout.
  int screenCount = ScreenCount(dpy);
  for (int idx = 0; idx < screenCount; idx++) {
    if (DefaultDepth(dpy, idx) != 24) {
      return false;
    }
  }

  // On at least amdgpu open source driver, eglInitialize fails unless
  // a valid XDisplay pointer is passed as the native display.
  if (!get_egl_status(dpy, true, pci_count != 1)) {
    return false;
  }

  // Get monitor and DDX driver information
  get_xrandr_info(dpy);

  // Bug 1715245: Closing the display connection here crashes on NV prop.
  // drivers. Just leave it open, the process will exit shortly after anyway.
  // XCloseDisplay(dpy);

  record_value("TEST_TYPE\nEGL\n");
  return true;
}

static void glxtest() {
  int gotGlxInfo = 0;
  int gotDriDriver = 0;

  get_glx_status(&gotGlxInfo, &gotDriDriver);
  if (!gotGlxInfo) {
    get_egl_status(nullptr, true, false);
  } else if (!gotDriDriver) {
    // If we failed to get the driver name from X, try via
    // EGL_MESA_query_driver. We are probably using Wayland.
    get_egl_status(nullptr, false, true);
  }

  record_value("TEST_TYPE\nGLX\n");
}
#endif

#ifdef MOZ_WAYLAND
static void wayland_egltest() {
  // NOTE: returns false to fall back to X11 when the Wayland socket doesn't
  // exist but fails with record_error if something actually went wrong
  struct wl_display* dpy = wl_display_connect(nullptr);
  if (!dpy) {
    record_error("Could not connect to wayland socket");
    return;
  }

  if (!get_egl_status((EGLNativeDisplayType)dpy, true, false)) {
    record_error("EGL test failed");
  }

  // This is enough to crash some broken NVIDIA prime + Wayland setups, see
  // https://github.com/NVIDIA/egl-wayland/issues/41 and bug 1768260.
  wl_display_roundtrip(dpy);

  wl_display_disconnect(dpy);
  record_value("TEST_TYPE\nEGL\n");
}

static constexpr struct {
  VAProfile mVAProfile;
  nsLiteralCString mName;
} kVAAPiProfileName[] = {
#  define MAP(v) \
    { VAProfile##v, nsLiteralCString(#v) }
    MAP(H264ConstrainedBaseline),
    MAP(H264Main),
    MAP(H264High),
    MAP(VP8Version0_3),
    MAP(VP9Profile0),
    MAP(VP9Profile2),
    MAP(AV1Profile0),
    MAP(AV1Profile1),
#  undef MAP
};

static const char* VAProfileName(VAProfile aVAProfile) {
  for (const auto& profile : kVAAPiProfileName) {
    if (profile.mVAProfile == aVAProfile) {
      return profile.mName.get();
    }
  }
  return nullptr;
}

int childvaapitest() {
  int renderDeviceFD = -1;
  VAProfile* profiles = nullptr;
  VAEntrypoint* entryPoints = nullptr;
  PRLibrary* libDrm = nullptr;
  VADisplay display = nullptr;

  auto autoRelease = mozilla::MakeScopeExit([&] {
    if (renderDeviceFD > -1) {
      close(renderDeviceFD);
    }
    delete[] profiles;
    delete[] entryPoints;
    if (display) {
      vaTerminate(display);
    }
    if (libDrm) {
      PR_UnloadLibrary(libDrm);
    }
  });

  renderDeviceFD = open(glxtest_render_device_path, O_RDWR);
  if (renderDeviceFD == -1) {
    return 3;
  }

  PRLibSpec lspec;
  lspec.type = PR_LibSpec_Pathname;
  const char* libName = "libva-drm.so.2";
  lspec.value.pathname = libName;
  libDrm = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
  if (!libDrm) {
    return 4;
  }

  static auto sVaGetDisplayDRM =
      (void* (*)(int fd))PR_FindSymbol(libDrm, "vaGetDisplayDRM");
  if (!sVaGetDisplayDRM) {
    return 5;
  }

  display = sVaGetDisplayDRM(renderDeviceFD);
  if (!display) {
    return 6;
  }

  int major, minor;
  VAStatus status = vaInitialize(display, &major, &minor);
  if (status != VA_STATUS_SUCCESS) {
    return 7;
  }

  int maxProfiles = vaMaxNumProfiles(display);
  int maxEntryPoints = vaMaxNumEntrypoints(display);
  if (MOZ_UNLIKELY(maxProfiles <= 0 || maxEntryPoints <= 0)) {
    return 8;
  }

  profiles = new VAProfile[maxProfiles];
  int numProfiles = 0;
  status = vaQueryConfigProfiles(display, profiles, &numProfiles);
  if (status != VA_STATUS_SUCCESS) {
    return 9;
  }
  numProfiles = std::min(numProfiles, maxProfiles);

  entryPoints = new VAEntrypoint[maxEntryPoints];
  for (int p = 0; p < numProfiles; p++) {
    VAProfile profile = profiles[p];

    // Check only supported profiles
    if (!VAProfileName(profile)) {
      continue;
    }

    int numEntryPoints = 0;
    status = vaQueryConfigEntrypoints(display, profile, entryPoints,
                                      &numEntryPoints);
    if (status != VA_STATUS_SUCCESS) {
      continue;
    }
    numEntryPoints = std::min(numEntryPoints, maxEntryPoints);

    for (int entry = 0; entry < numEntryPoints; entry++) {
      if (entryPoints[entry] != VAEntrypointVLD) {
        continue;
      }
      VAConfigID config = VA_INVALID_ID;
      status = vaCreateConfig(display, profile, entryPoints[entry], nullptr, 0,
                              &config);
      if (status == VA_STATUS_SUCCESS) {
        vaDestroyConfig(display, config);
        return 0;
      }
    }
  }
  return 10;
}

static void vaapitest() {
  if (!glxtest_render_device_path) {
    return;
  }

  pid_t vaapitest_pid = fork();
  if (vaapitest_pid == 0) {
    int vaapirv = childvaapitest();
    _exit(vaapirv);
  } else if (vaapitest_pid > 0) {
    int vaapitest_status = 0;
    bool wait_for_vaapitest_process = true;

    while (wait_for_vaapitest_process) {
      if (waitpid(vaapitest_pid, &vaapitest_status, 0) == -1) {
        wait_for_vaapitest_process = false;
        record_warning(
            "VA-API test failed: waiting for VA-API process failed.");
      } else if (WIFEXITED(vaapitest_status) || WIFSIGNALED(vaapitest_status)) {
        wait_for_vaapitest_process = false;
      }
    }

    if (WIFEXITED(vaapitest_status)) {
      switch (WEXITSTATUS(vaapitest_status)) {
        case 0:
          record_value("VAAPI_SUPPORTED\nTRUE\n");
          break;
        case 3:
          record_warning(
              "VA-API test failed: opening render device path failed.");
          break;
        case 4:
          record_warning(
              "VA-API test failed: missing or old libva-drm library.");
          break;
        case 5:
          record_warning("VA-API test failed: missing vaGetDisplayDRM.");
          break;
        case 6:
          record_warning("VA-API test failed: failed to get vaGetDisplayDRM.");
          break;
        case 7:
          record_warning(
              "VA-API test failed: failed to initialise VAAPI connection.");
          break;
        case 8:
          record_warning(
              "VA-API test failed: wrong VAAPI profiles/entry point nums.");
          break;
        case 9:
          record_warning("VA-API test failed: vaQueryConfigProfiles() failed.");
          break;
        case 10:
          record_warning(
              "VA-API test failed: no supported VAAPI profile found.");
          break;
        default:
          record_warning(
              "VA-API test failed: Something unexpected went wrong.");
          break;
      }
    } else {
      record_warning(
          "VA-API test failed: process crashed. Please check your VA-API "
          "drivers.");
    }
  } else {
    record_warning("VA-API test failed: Could not fork process.");
  }
}
#endif

int childgltest() {
  enum { bufsize = 2048 };
  char buf[bufsize];

  // We save it as a global so that the X error handler can flush the buffer
  // before early exiting.
  glxtest_buf = buf;
  glxtest_bufsize = bufsize;

  // Get a list of all GPUs from the PCI bus.
  int pci_count = get_pci_status();

#ifdef MOZ_WAYLAND
  if (IsWaylandEnabled()) {
    wayland_egltest();
  } else
#endif
  {
#ifdef MOZ_X11
    // TODO: --display command line argument is not properly handled
    if (!x11_egltest(pci_count)) {
      glxtest();
    }
#endif
  }

#ifdef MOZ_WAYLAND
  vaapitest();
#endif

  // Finally write buffered data to the pipe.
  record_flush();

  // If we completely filled the buffer, we need to tell the parent.
  if (glxtest_length >= glxtest_bufsize) {
    return EXIT_FAILURE_BUFFER_TOO_SMALL;
  }

  return EXIT_SUCCESS;
}

}  // extern "C"

/** \returns true in the child glxtest process, false in the parent process */
bool fire_glxtest_process() {
  int pfd[2];
  if (pipe(pfd) == -1) {
    perror("pipe");
    return false;
  }
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    close(pfd[0]);
    close(pfd[1]);
    return false;
  }
  // The child exits early to avoid running the full shutdown sequence and avoid
  // conflicting with threads we have already spawned (like the profiler).
  if (pid == 0) {
    close(pfd[0]);
    write_end_of_the_pipe = pfd[1];
    close_logging();
    int rv = childgltest();
    close(pfd[1]);
    _exit(rv);
  }

  close(pfd[1]);
  mozilla::widget::glxtest_pipe = pfd[0];
  mozilla::widget::glxtest_pid = pid;
  return false;
}
