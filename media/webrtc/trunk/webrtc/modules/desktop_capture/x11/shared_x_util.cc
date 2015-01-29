/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/x11/shared_x_util.h"

namespace webrtc {

WindowUtilX11::WindowUtilX11(scoped_refptr<SharedXDisplay> x_display) {
  x_display_ = x_display;
  wm_state_atom_ = XInternAtom(display(), "WM_STATE", True);
  window_type_atom_ = XInternAtom(display(), "_NET_WM_WINDOW_TYPE", True);
  normal_window_type_atom_ = XInternAtom(display(), "_NET_WM_WINDOW_TYPE_NORMAL", True);
  process_atom_ = XInternAtom(display(), "_NET_WM_PID", True);
  frame_extends_atom_ = XInternAtom(display(), "_NET_FRAME_EXTENTS", True);
}

WindowUtilX11::~WindowUtilX11() {
}

::Window WindowUtilX11::GetApplicationWindow(::Window window) {
  // Get WM_STATE property of the window.
  XWindowProperty<uint32_t> window_state(display(), window, wm_state_atom_);

  // WM_STATE is considered to be set to WithdrawnState when it missing.
  int32_t state = window_state.is_valid() ? *window_state.data() : WithdrawnState;

  if (state == NormalState) {
    // Window has WM_STATE==NormalState. Return it.
    return window;
  } else if (state == IconicState) {
    // Window is in minimized. Skip it.
    return 0;
  }

  // If the window is in WithdrawnState then look at all of its children.
  ::Window root, parent;
  ::Window *children;
  unsigned int num_children;
  if (!XQueryTree(display(), window, &root, &parent, &children, &num_children)) {
    LOG(LS_ERROR) << "Failed to query for child windows although window"
                  << "does not have a valid WM_STATE.";
    return 0;
  }
  ::Window app_window = 0;
  for (unsigned int i = 0; i < num_children; ++i) {
    app_window = GetApplicationWindow(children[i]);
    if (app_window) {
      break;
    }
  }

  if (children) {
    XFree(children);
  }
  return app_window;
}

bool WindowUtilX11::IsDesktopElement(::Window window) {
  if (window == 0) {
    return false;
  }

  // First look for _NET_WM_WINDOW_TYPE. The standard
  // (http://standards.freedesktop.org/wm-spec/latest/ar01s05.html#id2760306)
  // says this hint *should* be present on all windows, and we use the existence
  // of _NET_WM_WINDOW_TYPE_NORMAL in the property to indicate a window is not
  // a desktop element (that is, only "normal" windows should be shareable).
  XWindowProperty<uint32_t> window_type(display(), window, window_type_atom_);
  if (window_type.is_valid() && window_type.size() > 0) {
    uint32_t* end = window_type.data() + window_type.size();
    bool is_normal = (end != std::find(window_type.data(), end, normal_window_type_atom_));
    return !is_normal;
  }

  // Fall back on using the hint.
  XClassHint class_hint;
  Status status = XGetClassHint(display(), window, &class_hint);
  bool result = false;
  if (status == 0) {
    // No hints, assume this is a normal application window.
    return result;
  }

  if (strcmp("gnome-panel", class_hint.res_name) == 0 ||
      strcmp("desktop_window", class_hint.res_name) == 0) {
    result = true;
  }
  XFree(class_hint.res_name);
  XFree(class_hint.res_class);
  return result;
}

bool WindowUtilX11::GetWindowTitle(::Window window, std::string* title) {
  int status;
  bool result = false;
  XTextProperty window_name;
  window_name.value = NULL;
  if (window) {
    char * pWinName = NULL;
    if(XFetchName(display(), window, &pWinName)){
      *title = pWinName;
      XFree(pWinName);
      result = true;
    }
    else{
      status = XGetWMName(display(), window, &window_name);
      if (status && window_name.value && window_name.nitems) {
        int cnt;
        char **list = NULL;
        status = Xutf8TextPropertyToTextList(display(), &window_name, &list,
                                             &cnt);
        if (status >= Success && cnt && *list) {
          if (cnt > 1) {
            LOG(LS_INFO) << "Window has " << cnt << " text properties, only using the first one.";
          }
          *title = *list;
          result = true;
        }
        if (list) {
          XFreeStringList(list);
        }
      }
      if (window_name.value) {
        XFree(window_name.value);
      }
    }
  }
  return result;
}

bool WindowUtilX11::BringWindowToFront(::Window window) {
  if (!window) {
    return false;
  }

  unsigned int num_children;
  ::Window* children;
  ::Window parent;
  ::Window root;
  // Find the root window to pass event to.
  int status = XQueryTree(display(), window, &root, &parent, &children, &num_children);
  if (status == 0) {
    LOG(LS_ERROR) << "Failed to query for the root window.";
    return false;
  }

  if (children) {
    XFree(children);
  }

  XRaiseWindow(display(), window);

  // Some window managers (e.g., metacity in GNOME) consider it illegal to
  // raise a window without also giving it input focus with
  // _NET_ACTIVE_WINDOW, so XRaiseWindow() on its own isn't enough.
  Atom atom = XInternAtom(display(), "_NET_ACTIVE_WINDOW", True);
  if (atom != None) {
    XEvent xev;
    xev.xclient.type = ClientMessage;
    xev.xclient.serial = 0;
    xev.xclient.send_event = True;
    xev.xclient.window = window;
    xev.xclient.message_type = atom;

    // The format member is set to 8, 16, or 32 and specifies whether the
    // data should be viewed as a list of bytes, shorts, or longs.
    xev.xclient.format = 32;

    memset(xev.xclient.data.l, 0, sizeof(xev.xclient.data.l));

    XSendEvent(display(),
               root,
               False,
               SubstructureRedirectMask | SubstructureNotifyMask,
               &xev);
  }
  XFlush(display());
  return true;
}

int WindowUtilX11::GetWindowProcessID(::Window window) {
  // Get _NET_WM_PID property of the window.
  XWindowProperty<uint32_t> process_id(display(), window, process_atom_);

  return process_id.is_valid() ? *process_id.data() : 0;
}

int32_t WindowUtilX11::GetWindowStatus(::Window window) {
  // Get WM_STATE property of the window.
  XWindowProperty<uint32_t> window_state(display(), window, wm_state_atom_);

  // WM_STATE is considered to be set to -1 when it missing.
  int32_t state = window_state.is_valid() ? *window_state.data() : -1;
  return state;
}

bool WindowUtilX11::GetWindowFrameExtents(::Window window,
                                          int32_t &left, int32_t &top,
                                          int32_t &right, int32_t &bottom) {
  //reset it first
  left = top = right = bottom =0;

  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_remaining;
  unsigned char *data;
  int status;

  status = XGetWindowProperty(display(),
                              window,
                              frame_extends_atom_,
                              0,      // long_offset
                              4,      // long_length - we expect 4 32-bit values for _NET_FRAME_EXTENTS
                              False,  // delete
                              AnyPropertyType,
                              &actual_type,
                              &actual_format,
                              &nitems,
                              &bytes_remaining,
                              &data);

  if (status == Success) {
    if ((nitems == 4) && (bytes_remaining == 0)) {
      long *data_as_long = (long *)((void *) data);
      left   = (int) *(data_as_long++);
      right  = (int) *(data_as_long++);
      top    = (int) *(data_as_long++);
      bottom = (int) *(data_as_long++);
      return true;
    }
    XFree (data);
  }
  return false;
}

bool WindowUtilX11::GetWindowRect(::Window window, XRectangle & rcWindow, bool bWithFrame) {
  // reset
  rcWindow.x = rcWindow.y = rcWindow.width =  rcWindow.height = 0;

  // get window info
  XWindowAttributes win_info;
  if (!XGetWindowAttributes(display(), window, &win_info)) {
    return false;
  }

  int absx,absy;
  ::Window temp_win;
  ::Window root_win = DefaultRootWindow(display());
  if (!XTranslateCoordinates(display(), window, root_win, 0, 0, &absx, &absy, &temp_win)) {
    return false;
  }

  // Adjust to limit in screen
  XRectangle  screen_rect;
  int nScreenCX = DisplayWidth(display(), DefaultScreen(display()));
  int nScreenCY = DisplayHeight(display(), DefaultScreen(display()));
  screen_rect.x = 0;
  screen_rect.y = 0;
  screen_rect.width = nScreenCX;
  screen_rect.height = nScreenCY;

  if (absx < 0) {
    win_info.width += absx;
    absx = 0;
  } else if ((absx + win_info.width) > nScreenCX) {
    win_info.width = nScreenCX - absx;
  }
  if (absy < 0) {
    win_info.height += absy;
    absy = 0;
  } else if ((absy + win_info.height) > nScreenCY) {
    win_info.height = nScreenCY - absy;
  }

  // data setting
  rcWindow.x = absx;
  rcWindow.y = absy;
  rcWindow.width = win_info.width;
  rcWindow.height = win_info.height;

  if (bWithFrame) {
    int left;
    int right;
    int top;
    int bottom;
    if (GetWindowFrameExtents(window, left, top, right, bottom)) {
      rcWindow.x -= left;
      rcWindow.y -= top;
      rcWindow.width += (left + right);
      rcWindow.height += (top + bottom);
    }
  }
  return true;
}

}//namespace webrtc
