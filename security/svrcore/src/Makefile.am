EXTRA_DIST = key.ico \
	     logo.ico \
	     manifest.mn \
	     Makefile.win \
	     ntgetpin.rc

lib_LTLIBRARIES = libsvrcore.la
include_HEADERS = svrcore.h
libsvrcore_la_SOURCES =	alt.c \
		      	cache.c \
		      	errors.c \
		      	file.c \
			ntgetpin.c \
			ntresource.h \
			pin.c \
			pk11.c \
			std.c \
			user.c

libsvrcore_la_LDFLAGS = -version-info 0:0:0 @NSS_LIBS@ @NSPR_LIBS@
libsvrcore_la_CFLAGS = @NSS_CFLAGS@ @NSPR_CFLAGS@
