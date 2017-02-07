# Copyright (c) 2013-2014 The Khronos Group Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and/or associated documentation files (the
# "Materials"), to deal in the Materials without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Materials, and to
# permit persons to whom the Materials are furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Materials.
#
# THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.

# Generator scripts and options
# GENOPTS can be e.g. '-noprotect'

PYFILES = genheaders.py reg.py
GENOPTS =
GENHEADERS = genheaders.py $(GENOPTS)

# Generate all headers for GL / GLES / WGL / GLX / EGL
# Different headers depend on different XML registry files

GLHEADERS  = GL/glext.h GL/glcorearb.h \
	     GLES/gl.h GLES/glext.h \
	     GLES2/gl2.h GLES2/gl2ext.h \
	     GLES3/gl3.h
GLXHEADERS = GL/glx.h GL/glxext.h
WGLHEADERS = GL/wgl.h GL/wglext.h
EGLHEADERS = EGL/egl.h EGL/eglext.h
ALLHEADERS = $(GLHEADERS) $(GLXHEADERS) $(WGLHEADERS) $(EGLHEADERS)

default: $(ALLHEADERS)

$(GLHEADERS): gl.xml $(PYFILES)
	$(GENHEADERS) $@

$(GLXHEADERS): glx.xml $(PYFILES)
	$(GENHEADERS) $@ -registry glx.xml

$(WGLHEADERS): wgl.xml $(PYFILES)
	$(GENHEADERS) $@ -registry wgl.xml

# Not finished yet
$(EGLHEADERS): egl.xml $(PYFILES)
	$(GENHEADERS) $@ -registry egl.xml

# Generate Relax NG XML schema from Compact schema

registry.rng: registry.rnc
	trang registry.rnc registry.rng

# Verify all registry XML files against the schema

validate:
	jing -c registry.rnc gl.xml
	jing -c registry.rnc glx.xml
	jing -c registry.rnc wgl.xml
	jing -c registry.rnc egl.xml

clean:

clobber: clean
	-rm -f diag.txt dumpReg.txt errwarn.txt
