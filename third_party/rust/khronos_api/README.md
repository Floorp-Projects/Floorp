# khronos_api

[![Version](https://img.shields.io/crates/v/khronos_api.svg)](https://crates.io/crates/khronos_api)
[![License](https://img.shields.io/crates/l/khronos_api.svg)](https://github.com/brendanzab/gl-rs/blob/master/LICENSE)
[![Downloads](https://img.shields.io/crates/d/khronos_api.svg)](https://crates.io/crates/khronos_api)

The Khronos XML API Registry, exposed as byte string constants.

```toml
[build-dependencies]
khronos_api = "1.0.0"
```

The following constants are provided:

- `GL_XML`: the contents of [`gl.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/gl.xml)
- `EGL_XML`: the contents of [`egl.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/egl.xml)
- `WGL_XML`: the contents of [`wgl.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/wgl.xml)
- `GLX_XML`: the contents of [`glx.xml`](https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/glx.xml)

## Changelog

### v1.0.0

- Initial release
- Documentation improvements
