/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
#include <GL/gl.h>
#include <GL/glx.h>
#include <dlfcn.h>
#include "nscore.h"

#ifdef __SUNPRO_CC
#include <stdio.h>
#endif

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
  exit(EXIT_FAILURE);
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
  exit(EXIT_FAILURE);
  return 0;
}

static void glxtest()
{
  ///// Open libGL and load needed symbols /////
  void *libgl = dlopen("libGL.so.1", RTLD_LAZY);
  if (!libgl)
    fatal_error("Unable to load libGL.so.1");

  typedef GLXFBConfig* (* PFNGLXQUERYEXTENSION) (Display *, int *, int *);
  PFNGLXQUERYEXTENSION glXQueryExtension = cast<PFNGLXQUERYEXTENSION>(dlsym(libgl, "glXQueryExtension"));

  typedef GLXFBConfig* (* PFNGLXQUERYVERSION) (Display *, int *, int *);
  PFNGLXQUERYVERSION glXQueryVersion = cast<PFNGLXQUERYVERSION>(dlsym(libgl, "glXQueryVersion"));

  typedef GLXFBConfig* (* PFNGLXCHOOSEFBCONFIG) (Display *, int, const int *, int *);
  PFNGLXCHOOSEFBCONFIG glXChooseFBConfig = cast<PFNGLXCHOOSEFBCONFIG>(dlsym(libgl, "glXChooseFBConfig"));

  typedef XVisualInfo* (* PFNGLXGETVISUALFROMFBCONFIG) (Display *, GLXFBConfig);
  PFNGLXGETVISUALFROMFBCONFIG glXGetVisualFromFBConfig = cast<PFNGLXGETVISUALFROMFBCONFIG>(dlsym(libgl, "glXGetVisualFromFBConfig"));

  typedef GLXPixmap (* PFNGLXCREATEPIXMAP) (Display *, GLXFBConfig, Pixmap, const int *);
  PFNGLXCREATEPIXMAP glXCreatePixmap = cast<PFNGLXCREATEPIXMAP>(dlsym(libgl, "glXCreatePixmap"));

  typedef GLXContext (* PFNGLXCREATENEWCONTEXT) (Display *, GLXFBConfig, int, GLXContext, Bool);
  PFNGLXCREATENEWCONTEXT glXCreateNewContext = cast<PFNGLXCREATENEWCONTEXT>(dlsym(libgl, "glXCreateNewContext"));

  typedef Bool (* PFNGLXMAKECURRENT) (Display*, GLXDrawable, GLXContext);
  PFNGLXMAKECURRENT glXMakeCurrent = cast<PFNGLXMAKECURRENT>(dlsym(libgl, "glXMakeCurrent"));

  typedef void (* PFNGLXDESTROYPIXMAP) (Display *, GLXPixmap);
  PFNGLXDESTROYPIXMAP glXDestroyPixmap = cast<PFNGLXDESTROYPIXMAP>(dlsym(libgl, "glXDestroyPixmap"));

  typedef void (* PFNGLXDESTROYCONTEXT) (Display*, GLXContext);
  PFNGLXDESTROYCONTEXT glXDestroyContext = cast<PFNGLXDESTROYCONTEXT>(dlsym(libgl, "glXDestroyContext"));

  typedef GLubyte* (* PFNGLGETSTRING) (GLenum);
  PFNGLGETSTRING glGetString = cast<PFNGLGETSTRING>(dlsym(libgl, "glGetString"));

  typedef void* (* PFNGLXGETPROCADDRESS) (const char *);
  PFNGLXGETPROCADDRESS glXGetProcAddress = cast<PFNGLXGETPROCADDRESS>(dlsym(libgl, "glXGetProcAddress"));

  if (!glXQueryExtension ||
      !glXQueryVersion ||
      !glXChooseFBConfig ||
      !glXGetVisualFromFBConfig ||
      !glXCreatePixmap ||
      !glXCreateNewContext ||
      !glXMakeCurrent ||
      !glXDestroyPixmap ||
      !glXDestroyContext ||
      !glGetString ||
      !glXGetProcAddress)
  {
    fatal_error("Unable to find required symbols in libGL.so.1");
  }
  ///// Open a connection to the X server /////
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy)
    fatal_error("Unable to open a connection to the X server");
  
  ///// Check that the GLX extension is present /////
  if (!glXQueryExtension(dpy, NULL, NULL))
    fatal_error("GLX extension missing");
  
  ///// Check that the GLX version is >= 1.3, needed for glXCreatePixmap, bug 659932 /////
  int majorVersion, minorVersion;
  if (!glXQueryVersion(dpy, &majorVersion, &minorVersion))
    fatal_error("Unable to query GLX version");

  if (majorVersion < 1 || (majorVersion == 1 && minorVersion < 3))
    fatal_error("GLX version older than the required 1.3");

  XSetErrorHandler(x_error_handler);

  ///// Get a FBConfig and a visual /////
  int attribs[] = {
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    GLX_X_RENDERABLE, True,
    0
  };
  int numReturned;
  GLXFBConfig *fbConfigs = glXChooseFBConfig(dpy, DefaultScreen(dpy), attribs, &numReturned );
  if (!fbConfigs)
    fatal_error("No FBConfigs found");
  XVisualInfo *vInfo = glXGetVisualFromFBConfig(dpy, fbConfigs[0]);
  if (!vInfo)
    fatal_error("No visual found for first FBConfig");

  ///// Get a Pixmap and a GLXPixmap /////
  Pixmap pixmap = XCreatePixmap(dpy, RootWindow(dpy, vInfo->screen), 4, 4, 32);
  GLXPixmap glxpixmap = glXCreatePixmap(dpy, fbConfigs[0], pixmap, NULL);

  ///// Get a GL context and make it current //////
  GLXContext context = glXCreateNewContext(dpy, fbConfigs[0], GLX_RGBA_TYPE, NULL, True);
  glXMakeCurrent(dpy, glxpixmap, context);

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

  ///// Check that no X error happened /////
  // In case of X errors, our X error handler will exit() now.
  // We really want to make sure that the system is able to create a GL context without generating X errors,
  // as these would crash the application.
  XSync(dpy, False);
  
  ///// Finally write data to the pipe /////
  write(write_end_of_the_pipe, buf, length);

  ///// Clean up. Indeed, the parent process might fail to kill us (e.g. if it doesn't need to check GL info)
  ///// so we might be staying alive for longer than expected, so it's important to consume as little memory as
  ///// possible. Also we want to check that we're able to do that too without generating X errors.
  glXMakeCurrent(dpy, None, NULL); // must release the GL context before destroying it
  glXDestroyContext(dpy, context);
  glXDestroyPixmap(dpy, glxpixmap);
  XFreePixmap(dpy, pixmap);
  XCloseDisplay(dpy);
  dlclose(libgl);
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
  if (pid == 0) {
      close(pfd[0]);
      write_end_of_the_pipe = pfd[1];
      glxtest();
      close(pfd[1]);
      return true;
  }

  close(pfd[1]);
  mozilla::widget::glxtest_pipe = pfd[0];
  mozilla::widget::glxtest_pid = pid;
  return false;
}
