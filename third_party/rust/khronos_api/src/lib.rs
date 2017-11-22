// Copyright 2015 Brendan Zabarauskas and the gl-rs developers
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

/// The contents of [`gl.xml`](https://github.com/KhronosGroup/OpenGL-Registry/blob/master/xml/gl.xml)
pub const GL_XML: &'static [u8] = include_bytes!("../api/xml/gl.xml");

/// The contents of [`egl.xml`](https://github.com/KhronosGroup/EGL-Registry/blob/master/api/egl.xml)
pub const EGL_XML: &'static [u8] = include_bytes!("../api_egl/api/egl.xml");

/// The contents of [`wgl.xml`](https://github.com/KhronosGroup/OpenGL-Registry/blob/master/xml/wgl.xml)
pub const WGL_XML: &'static [u8] = include_bytes!("../api/xml/wgl.xml");

/// The contents of [`glx.xml`](https://github.com/KhronosGroup/OpenGL-Registry/blob/master/xml/glx.xml)
pub const GLX_XML: &'static [u8] = include_bytes!("../api/xml/glx.xml");
