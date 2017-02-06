// Copyright 2015 Brendan Zabarauskas
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! This crates contains the sources of the official OpenGL repository.

/// The contents of [`gl.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/gl.xml)
pub const GL_XML: &'static [u8] = include_bytes!("../api/gl.xml");

/// The contents of [`egl.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/egl.xml)
pub const EGL_XML: &'static [u8] = include_bytes!("../api/egl.xml");

/// The contents of [`wgl.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/wgl.xml)
pub const WGL_XML: &'static [u8] = include_bytes!("../api/wgl.xml");

/// The contents of [`glx.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/glx.xml)
pub const GLX_XML: &'static [u8] = include_bytes!("../api/glx.xml");
