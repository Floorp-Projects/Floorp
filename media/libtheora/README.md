# Xiph.org Foundation's libtheora

### What is Theora?

Theora was Xiph.Org's first publicly released video codec, intended
for use within the Foundation's Ogg multimedia streaming system.
Theora is derived directly from On2's VP3 codec, adds new features
while allowing it a longer useful lifetime.

The 1.0 release decoder supported all the new features, but the
encoder is nearly identical to the VP3 code.

The 1.1 release featured a completely rewritten encoder, offering
better performance and compression, and making more complete use
of the format's feature set.

The 1.2 release features significant additional improvements in
compression and performance. Files produced by newer encoders can
be decoded by earlier releases.

### Where is Theora?

Theora's main site is https://www.theora.org. Releases of Theora
and related libraries can be found on the
[download page](https://www.theora.org/downloads/) or the
[main Xiph.Org site](https://xiph.org/downloads/).

Development source is kept at https://gitlab.xiph.org/xiph/theora.

## Getting started with the code

### What do I need to build the source?

Requirements summary:

For libtheora:

*   libogg 1.1 or newer.

For example encoder:

*   as above,
*   libvorbis and libvorbisenc 1.0.1 or newer.
    (libvorbis 1.3.1 or newer for 5.1 audio)

For creating a source distribution package:

*   as above,
*   Doxygen to build the API documentation,
*   pdflatex and fig2dev to build the format specification
    (transfig package in Ubuntu).

For the player only:

*   as above,
*   SDL (Simple Direct media Layer) libraries and headers,
*   OSS audio driver and development headers.

The provided build system is the GNU automake/autoconf system, and
the main library, libtheora, should already build smoothly on any
system.  Failure of libtheora to build on a GNU-enabled system is
considered a bug; please report problems to theora-dev@xiph.org.

Windows build support is included in the win32 directory.

Project files for Apple XCode are included in the macosx directory.

There is also a more limited scons build.

### How do I use the sample encoder?

The sample encoder takes raw video in YUV4MPEG2 format, as used by
lavtools, mjpeg-tools and other packages. The encoder expects audio,
if any, in a separate wave WAV file. Try 'encoder_example -h' for a
complete list of options.

An easy way to get raw video and audio files is to use MPlayer as an
export utility.  The options " -ao pcm -vo yuv4mpeg " will export a
wav file named audiodump.wav and a YUV video file in the correct
format for encoder_example as stream.yuv.  Be careful when exporting
video alone; MPlayer may drop frames to 'keep up' with the audio
timer.  The example encoder can't properly synchronize input audio and
video file that aren't in sync to begin with.

The encoder will also take video or audio on stdin if '-' is specified
as the input file name.

There is also a 'png2theora' example which accepts a set of image
files in that format.

### How do I use the sample player?

The sample player takes an Ogg file on standard in; the file may be
audio alone, video alone or video with audio.

### What other tools are available?

The programs in the examples directory are intended as tutorial source
for developers using the library. As such they sacrifice features and
robustness in the interests of comprehension and should not be
considered serious applications.

If you're wanting to just use theora, consider the programs linked
from https://www.theora.org/. There is playback support in a number
of common free players, and plugins for major media frameworks.
Jan Gerber's ffmpeg2theora is an excellent encoding front end.

## Troubleshooting the build process

### Compile error, such as:

encoder_internal.h:664: parse error before `ogg_uint16_t`

This means you have version of libogg prior to 1.1. A *complete* new Ogg
install, libs and headers is needed.

Also be sure that there aren't multiple copies of Ogg installed in
/usr and /usr/local; an older one might be first on the search path
for libs and headers.

### Link error, such as:

undefined reference to `oggpackB_stream`

See above; you need libogg 1.1 or later.

### Link error, such as:

undefined reference to `vorbis_granule_time`

You need libvorbis and libvorbisenc from the 1.0.1 release or later.

### Link error, such as:

/usr/lib/libSDL.a(SDL_esdaudio.lo): In function `ESD_OpenAudio`:
SDL_esdaudio.lo(.text+0x25d): undefined reference to `esd_play_stream`

Be sure to use an SDL that's built to work with OSS.  If you use an
SDL that is also built with ESD and/or ALSA support, it will try to
suck in all those extra libraries at link time too.  That will only
work if the extra libraries are also installed.

### Link warning, such as:

libtool: link: warning: library `/usr/lib/libogg.la` was moved.
libtool: link: warning: library `/usr/lib/libogg.la` was moved.

Re-run theora/autogen.sh after an Ogg or Vorbis rebuild/reinstall
