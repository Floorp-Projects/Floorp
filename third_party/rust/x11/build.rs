// x11-rs: Rust bindings for X11 libraries
// The X11 libraries are available under the MIT license.
// These bindings are public domain.

extern crate pkg_config;

fn main () {
  if cfg!(feature="dpms") { pkg_config::find_library("xext").unwrap(); }
  if cfg!(feature="glx") { pkg_config::find_library("gl").unwrap(); }
  if cfg!(feature="xcursor") { pkg_config::find_library("xcursor").unwrap(); }
  if cfg!(feature="xf86vmode") { pkg_config::find_library("xxf86vm").unwrap(); }
  if cfg!(feature="xft") { pkg_config::find_library("xft").unwrap(); }
  if cfg!(feature="xinerama") { pkg_config::find_library("xinerama").unwrap(); }
  if cfg!(feature="xinput") { pkg_config::find_library("xi").unwrap(); }
  if cfg!(feature="xlib") { pkg_config::find_library("x11").unwrap(); }
  if cfg!(feature="xlib_xcb") { pkg_config::find_library("x11-xcb").unwrap(); }
  if cfg!(feature="xmu") { pkg_config::find_library("xmu").unwrap(); }
  if cfg!(feature="xrandr") { pkg_config::find_library("xrandr").unwrap(); }
  if cfg!(feature="xrecord") { pkg_config::find_library("xtst").unwrap(); }
  if cfg!(feature="xrender") { pkg_config::find_library("xrender").unwrap(); }
  if cfg!(feature="xss") { pkg_config::find_library("xscrnsaver").unwrap(); }
  if cfg!(feature="xt") { pkg_config::find_library("xt").unwrap(); }
  if cfg!(feature="xtest") { pkg_config::find_library("xtst").unwrap(); }
}
