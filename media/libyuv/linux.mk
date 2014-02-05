# This is a generic makefile for libyuv for gcc.
# make -f linux.mk CC=clang++

CC=g++
CCFLAGS=-O2 -fomit-frame-pointer -Iinclude/

LOCAL_OBJ_FILES := \
    source/compare.o           \
    source/compare_common.o    \
    source/compare_posix.o     \
    source/convert.o           \
    source/convert_argb.o      \
    source/convert_from.o      \
    source/convert_from_argb.o \
    source/convert_to_argb.o   \
    source/convert_to_i420.o   \
    source/cpu_id.o            \
    source/format_conversion.o \
    source/planar_functions.o  \
    source/rotate.o            \
    source/rotate_argb.o       \
    source/rotate_mips.o       \
    source/row_any.o           \
    source/row_common.o        \
    source/row_mips.o          \
    source/row_posix.o         \
    source/scale.o             \
    source/scale_argb.o        \
    source/scale_common.o      \
    source/scale_mips.o        \
    source/scale_posix.o       \
    source/video_common.o

.cc.o:
	$(CC) -c $(CCFLAGS) $*.cc -o $*.o

all: libyuv.a convert linux.mk

libyuv.a: $(LOCAL_OBJ_FILES) linux.mk
	$(AR) $(ARFLAGS) -o $@ $(LOCAL_OBJ_FILES)

# A test utility that uses libyuv conversion.
convert: util/convert.cc linux.mk
	$(CC) $(CCFLAGS) -Iutil/ -o $@ util/convert.cc libyuv.a

clean:
	/bin/rm -f source/*.o *.ii *.s libyuv.a convert

