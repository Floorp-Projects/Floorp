/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//////////////////////////////////////////////////////////////////////////////
//
// Explanation: See bug 639842. Safely getting GL driver info on X11 is hard, because the only way to do
// that is to create a GL context and call glGetString(), but with bad drivers,
// just creating a GL context may crash.
//
// This file implements the idea to do that in a separate process.
//
// The only non-static function here is fire_glxtest_process(). It creates a pipe, publishes its 'read' end as the
// mozilla::widget::glxtest_pipe global variable, forks, and runs that GLX probe in the child process,
// which runs the glxtest() static function. This creates a X connection, a GLX context, calls glGetString, and writes that
// to the 'write' end of the pipe.

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <dlfcn.h>
#include "nscore.h"
#include <fcntl.h>
#include "stdint.h"

#ifdef __SUNPRO_CC
#include <stdio.h>
#endif

#include "X11/Xlib.h"
#include "X11/Xutil.h"

// stuff from glx.h
typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
#define GLX_RGBA        4
#define GLX_RED_SIZE    8
#define GLX_GREEN_SIZE  9
#define GLX_BLUE_SIZE   10

// stuff from gl.h
typedef uint8_t GLubyte;
typedef uint32_t GLenum;
#define GL_VENDOR       0x1F00
#define GL_RENDERER     0x1F01
#define GL_VERSION      0x1F02

namespace mozilla {
namespace widget {
// the read end of the pipe, which will be used by GfxInfo
extern int glxtest_pipe;
// the PID of the glxtest process, to pass to waitpid()
extern pid_t glxtest_pid;
}
}

// the write end of the pipe, which we're going to write to
static int write_end_of_the_pipe = -1;

// C++ standard collides with C standard in that it doesn't allow casting void* to function pointer types.
// So the work-around is to convert first to size_t.
// http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
template<typename func_ptr_type>
static func_ptr_type cast(void *ptr)
{
  return reinterpret_cast<func_ptr_type>(
           reinterpret_cast<size_t>(ptr)
         );
}

static void fatal_error(const char *str)
{
  write(write_end_of_the_pipe, str, strlen(str));
  write(write_end_of_the_pipe, "\n", 1);
  _exit(EXIT_FAILURE);
}

static int
x_error_handler(Display *, XErrorEvent *ev)
{
  enum { bufsize = 1024 };
  char buf[bufsize];
  int length = snprintf(buf, bufsize,
                        "X error occurred in GLX probe, error_code=%d, request_code=%d, minor_code=%d\n",
                        ev->error_code,
                        ev->request_code,
                        ev->minor_code);
  write(write_end_of_the_pipe, buf, length);
  _exit(EXIT_FAILURE);
  return 0;
}

static void glxtest()
{
  // we want to redirect to /dev/null stdout, stderr, and while we're at it,
  // any PR logging file descriptors. To that effect, we redirect all positive
  // file descriptors up to what open() returns here. In particular, 1 is stdout and 2 is stderr.
  int fd = open("/dev/null", O_WRONLY);
  for (int i = 1; i < fd; i++)
    dup2(fd, i);
  close(fd);

  if (getenv("MOZ_AVOID_OPENGL_ALTOGETHER"))
    fatal_error("The MOZ_AVOID_OPENGL_ALTOGETHER environment variable is defined");

  ///// Open libGL and load needed symbols /////
#ifdef __OpenBSD__
  #define LIBGL_FILENAME "libGL.so"
#else
  #define LIBGL_FILENAME "libGL.so.1"
#endif
  void *libgl = dlopen(LIBGL_FILENAME, RTLD_LAZY);
  if (!libgl)
    fatal_error("Unable to load " LIBGL_FILENAME);
  
  typedef void* (* PFNGLXGETPROCADDRESS) (const char *);
  PFNGLXGETPROCADDRESS glXGetProcAddress = cast<PFNGLXGETPROCADDRESS>(dlsym(libgl, "glXGetProcAddress"));
  
  if (!glXGetProcAddress)
    fatal_error("Unable to find glXGetProcAddress in " LIBGL_FILENAME);

  typedef GLXFBConfig* (* PFNGLXQUERYEXTENSION) (Display *, int *, int *);
  PFNGLXQUERYEXTENSION glXQueryExtension = cast<PFNGLXQUERYEXTENSION>(glXGetProcAddress("glXQueryExtension"));

  typedef GLXFBConfig* (* PFNGLXQUERYVERSION) (Display *, int *, int *);
  PFNGLXQUERYVERSION glXQueryVersion = cast<PFNGLXQUERYVERSION>(dlsym(libgl, "glXQueryVersion"));

  typedef XVisualInfo* (* PFNGLXCHOOSEVISUAL) (Display *, int, int *);
  PFNGLXCHOOSEVISUAL glXChooseVisual = cast<PFNGLXCHOOSEVISUAL>(glXGetProcAddress("glXChooseVisual"));

  typedef GLXContext (* PFNGLXCREATECONTEXT) (Display *, XVisualInfo *, GLXContext, Bool);
  PFNGLXCREATECONTEXT glXCreateContext = cast<PFNGLXCREATECONTEXT>(glXGetProcAddress("glXCreateContext"));

  typedef Bool (* PFNGLXMAKECURRENT) (Display*, GLXDrawable, GLXContext);
  PFNGLXMAKECURRENT glXMakeCurrent = cast<PFNGLXMAKECURRENT>(glXGetProcAddress("glXMakeCurrent"));

  typedef void (* PFNGLXDESTROYCONTEXT) (Display*, GLXContext);
  PFNGLXDESTROYCONTEXT glXDestroyContext = cast<PFNGLXDESTROYCONTEXT>(glXGetProcAddress("glXDestroyContext"));

  typedef GLubyte* (* PFNGLGETSTRING) (GLenum);
  PFNGLGETSTRING glGetString = cast<PFNGLGETSTRING>(glXGetProcAddress("glGetString"));

  if (!glXQueryExtension ||
      !glXQueryVersion ||
      !glXChooseVisual ||
      !glXCreateContext ||
      !glXMakeCurrent ||
      !glXDestroyContext ||
      !glGetString)
  {
    fatal_error("glXGetProcAddress couldn't find required functions");
  }
  ///// Open a connection to the X server /////
  Display *dpy = XOpenDisplay(nullptr);
  if (!dpy)
    fatal_error("Unable to open a connection to the X server");
  
  ///// Check that the GLX extension is present /////
  if (!glXQueryExtension(dpy, nullptr, nullptr))
    fatal_error("GLX extension missing");

  XSetErrorHandler(x_error_handler);

  ///// Get a visual /////
   int attribs[] = {
      GLX_RGBA,
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      None };
  XVisualInfo *vInfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
  if (!vInfo)
    fatal_error("No visuals found");

  // using a X11 Window instead of a GLXPixmap does not crash
  // fglrx in indirect rendering. bug 680644
  Window window;
  XSetWindowAttributes swa;
  swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vInfo->screen),
                                 vInfo->visual, AllocNone);

  swa.border_pixel = 0;
  window = XCreateWindow(dpy, RootWindow(dpy, vInfo->screen),
                       0, 0, 16, 16,
                       0, vInfo->depth, InputOutput, vInfo->visual,
                       CWBorderPixel | CWColormap, &swa);

  ///// Get a GL context and make it current //////
  GLXContext context = glXCreateContext(dpy, vInfo, nullptr, True);
  glXMakeCurrent(dpy, window, context);

  ///// Look for this symbol to determine texture_from_pixmap support /////
  void* glXBindTexImageEXT = glXGetProcAddress("glXBindTexImageEXT"); 

  ///// Get GL vendor/renderer/versions strings /////
  enum { bufsize = 1024 };
  char buf[bufsize];
  const GLubyte *vendorString = glGetString(GL_VENDOR);
  const GLubyte *rendererString = glGetString(GL_RENDERER);
  const GLubyte *versionString = glGetString(GL_VERSION);

  if (!vendorString || !rendererString || !versionString)
    fatal_error("glGetString returned null");

  int length = snprintf(buf, bufsize,
                        "VENDOR\n%s\nRENDERER\n%s\nVERSION\n%s\nTFP\n%s\n",
                        vendorString,
                        rendererString,
                        versionString,
                        glXBindTexImageEXT ? "TRUE" : "FALSE");
  if (length >= bufsize)
    fatal_error("GL strings length too large for buffer size");

  ///// Clean up. Indeed, the parent process might fail to kill us (e.g. if it doesn't need to check GL info)
  ///// so we might be staying alive for longer than expected, so it's important to consume as little memory as
  ///// possible. Also we want to check that we're able to do that too without generating X errors.
  glXMakeCurrent(dpy, None, nullptr); // must release the GL context before destroying it
  glXDestroyContext(dpy, context);
  XDestroyWindow(dpy, window);
  XFreeColormap(dpy, swa.colormap);

#ifdef NS_FREE_PERMANENT_DATA // conditionally defined in nscore.h, don't forget to #include it above
  XCloseDisplay(dpy);
#else
  // This XSync call wanted to be instead:
  //   XCloseDisplay(dpy);
  // but this can cause 1-minute stalls on certain setups using Nouveau, see bug 973192
  XSync(dpy, False);
#endif

  dlclose(libgl);

  ///// Finally write data to the pipe
  write(write_end_of_the_pipe, buf, length);
}

/** \returns true in the child glxtest process, false in the parent process */
bool fire_glxtest_process()
{
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
  // The child exits early to avoid running the full shutdown sequence and avoid conflicting with threads 
  // we have already spawned (like the profiler).
  if (pid == 0) {
      close(pfd[0]);
      write_end_of_the_pipe = pfd[1];
      glxtest();
      close(pfd[1]);
      _exit(0);
  }

  close(pfd[1]);
  mozilla::widget::glxtest_pipe = pfd[0];
  mozilla::widget::glxtest_pid = pid;
  return false;
}
