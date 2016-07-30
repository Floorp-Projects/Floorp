# Usage: sh update.sh <upstream_src_directory>
set -e

cp $1/AUTHORS .
cp $1/LICENSE .
cp $1/README.md .
cp $1/include/cubeb/cubeb.h include
cp $1/src/android/audiotrack_definitions.h src/android
cp $1/src/android/sles_definitions.h src/android
cp $1/src/cubeb-internal.h src
cp $1/src/cubeb-speex-resampler.h src
cp $1/src/cubeb.c src
cp $1/src/cubeb_alsa.c src
cp $1/src/cubeb_audiotrack.c src
cp $1/src/cubeb_audiounit.cpp src
cp $1/src/cubeb_osx_run_loop.h src
cp $1/src/cubeb_jack.cpp src
cp $1/src/cubeb_opensl.c src
cp $1/src/cubeb_panner.cpp src
cp $1/src/cubeb_panner.h src
cp $1/src/cubeb_pulse.c src
cp $1/src/cubeb_resampler.cpp src
cp $1/src/cubeb_resampler.h src
cp $1/src/cubeb_resampler_internal.h src
cp $1/src/cubeb_ring_array.h src
cp $1/src/cubeb_sndio.c src
cp $1/src/cubeb_utils.h src
cp $1/src/cubeb_utils_unix.h src
cp $1/src/cubeb_utils_win.h src
cp $1/src/cubeb_wasapi.cpp src
cp $1/src/cubeb_winmm.c src
cp $1/test/common.h tests/common.h
cp $1/test/test_audio.cpp tests/test_audio.cpp
#cp $1/test/test_devices.c tests/test_devices.cpp
cp $1/test/test_duplex.cpp tests/test_duplex.cpp
cp $1/test/test_latency.cpp tests/test_latency.cpp
cp $1/test/test_record.cpp tests/test_record.cpp
cp $1/test/test_resampler.cpp tests/test_resampler.cpp
cp $1/test/test_sanity.cpp tests/test_sanity.cpp
cp $1/test/test_tone.cpp tests/test_tone.cpp
cp $1/test/test_utils.cpp tests/test_utils.cpp

if [ -d $1/.git ]; then
  rev=$(cd $1 && git rev-parse --verify HEAD)
  dirty=$(cd $1 && git diff-index --name-only HEAD)
fi

if [ -n "$rev" ]; then
  version=$rev
  if [ -n "$dirty" ]; then
    version=$version-dirty
    echo "WARNING: updating from a dirty git repository."
  fi
  sed -i.bak -e "/The git commit ID used was/ s/[0-9a-f]\{40\}\(-dirty\)\{0,1\}\./$version./" README_MOZILLA
  rm README_MOZILLA.bak
else
  echo "Remember to update README_MOZILLA with the version details."
fi

