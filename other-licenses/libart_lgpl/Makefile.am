noinst_PROGRAMS = testart testuta gen_art_config

bin_SCRIPTS = \
	libart2-config

BUILT_SOURCES = art_config.h

art_config.h:	gen_art_config
	./gen_art_config > art_config.h

EXTRA_DIST = 			\
	libart-config.in	\
	libart-2.0.pc.in

lib_LTLIBRARIES = libart_lgpl_2.la

if OS_WIN32
no_undefined = -no-undefined

install-libtool-import-lib:
	$(INSTALL) .libs/libart_lgpl_2.dll.a $(DESTDIR)$(libdir)
uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libart_lgpl_2.dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

if MS_LIB_AVAILABLE
noinst_DATA = art_lgpl_2.lib

install-ms-lib:
	$(INSTALL) art_lgpl_2.lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/art_lgpl_2.lib

art_lgpl_2.lib : libart_lgpl_2.la
	lib -name:libart_lgpl_2-@LIBART_MAJOR_VERSION@.dll -def:.libs/libart_lgpl_2-@LIBART_MAJOR_VERSION@.dll-def -out:$@

else
install-ms-lib:
uninstall-ms-lib:
endif

libart_lgpl_2_la_SOURCES = \
	art_affine.c \
	art_alphagamma.c \
	art_bpath.c \
	art_gray_svp.c \
	art_misc.c \
	art_pixbuf.c \
	art_rect.c \
	art_rect_svp.c \
	art_rect_uta.c \
	art_render.c \
	art_render_gradient.c \
	art_render_mask.c \
	art_render_svp.c \
	art_rgb.c \
	art_rgb_affine.c \
	art_rgb_affine_private.c \
	art_rgb_affine_private.h \
	art_rgb_bitmap_affine.c \
	art_rgb_pixbuf_affine.c \
	art_rgb_rgba_affine.c \
	art_rgb_a_affine.c \
	art_rgba.c \
	art_rgb_svp.c \
	art_svp.c \
	art_svp_intersect.c \
	art_svp_ops.c \
	art_svp_point.c \
	art_svp_render_aa.c \
	art_svp_vpath.c \
	art_svp_vpath_stroke.c \
	art_svp_wind.c \
	art_uta.c \
	art_uta_ops.c \
	art_uta_rect.c \
	art_uta_vpath.c \
	art_uta_svp.c \
	art_vpath.c \
	art_vpath_bpath.c \
	art_vpath_dash.c \
	art_vpath_svp.c	\
	libart-features.c

libart_lgpl_2_la_LDFLAGS = -version-info @LIBART_VERSION_INFO@ $(no_undefined)
libart_lgpl_2_la_LIBADD = -lm

libart_lgplincdir = $(includedir)/libart-2.0/libart_lgpl
libart_lgplinc_HEADERS = \
	art_affine.h \
	art_alphagamma.h \
	art_bpath.h \
	art_config.h \
	art_filterlevel.h \
	art_gray_svp.h \
	art_misc.h \
	art_pathcode.h \
	art_pixbuf.h \
	art_point.h \
	art_rect.h \
	art_rect_svp.h \
	art_rect_uta.h \
	art_render.h \
	art_render_gradient.h \
	art_render_mask.h \
	art_render_svp.h \
	art_rgb.h \
	art_rgb_affine.h \
	art_rgb_bitmap_affine.h \
	art_rgb_pixbuf_affine.h \
	art_rgb_rgba_affine.h \
	art_rgb_a_affine.h \
	art_rgb_svp.h \
	art_rgba.h \
	art_svp.h \
	art_svp_intersect.h \
	art_svp_ops.h \
	art_svp_point.h \
	art_svp_render_aa.h \
	art_svp_vpath.h \
	art_svp_vpath_stroke.h \
	art_svp_wind.h \
	art_uta.h \
	art_uta_ops.h \
	art_uta_rect.h \
	art_uta_vpath.h \
	art_uta_svp.h \
	art_vpath.h \
	art_vpath_bpath.h \
	art_vpath_dash.h \
	art_vpath_svp.h	\
	libart.h	\
	libart-features.h

INCLUDES = -I$(top_srcdir) -I$(top_builddir) -DLIBART_COMPILATION

DEPS = $(top_builddir)/libart_lgpl_2.la
LDADDS = $(top_builddir)/libart_lgpl_2.la

testart_SOURCES=testart.c
testart_LDFLAGS =
testart_DEPENDENCIES = $(DEPS)
testart_LDADD = $(LDADDS) -lm

testuta_SOURCES=testuta.c
testuta_LDFLAGS =
testuta_DEPENDENCIES = $(DEPS)
testuta_LDADD = $(LDADDS) -lm

tests:	testart testuta

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libart-2.0.pc

libart2-config: libart-config
	cp -f libart-config libart2-config

install-data-local: install-ms-lib install-libtool-import-lib

uninstall-local: uninstall-ms-lib uninstall-libtool-import-lib
