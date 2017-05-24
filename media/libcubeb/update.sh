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
cp $1/src/cubeb_array_queue.h src
cp $1/src/cubeb_audiotrack.c src
cp $1/src/cubeb_audiounit.cpp src
cp $1/src/cubeb_jack.cpp src
cp $1/src/cubeb_log.cpp src
cp $1/src/cubeb_log.h src
cp $1/src/cubeb_mixer.cpp src
cp $1/src/cubeb_mixer.h src
cp $1/src/cubeb_opensl.c src
cp $1/src/cubeb_osx_run_loop.h src
cp $1/src/cubeb_panner.cpp src
cp $1/src/cubeb_panner.h src
cp $1/src/cubeb_pulse.c src
cp $1/src/cubeb_resampler.cpp src
cp $1/src/cubeb_resampler.h src
cp $1/src/cubeb_resampler_internal.h src
cp $1/src/cubeb_ring_array.h src
cp $1/src/cubeb_ringbuffer.h src
cp $1/src/cubeb_sndio.c src
cp $1/src/cubeb_utils.c src
cp $1/src/cubeb_utils.h src
cp $1/src/cubeb_utils_unix.h src
cp $1/src/cubeb_utils_win.h src
cp $1/src/cubeb_wasapi.cpp src
cp $1/src/cubeb_winmm.c src
cp $1/test/common.h gtest
cp $1/test/test_audio.cpp gtest
cp $1/test/test_devices.cpp gtest
cp $1/test/test_duplex.cpp gtest
cp $1/test/test_latency.cpp gtest
cp $1/test/test_mixer.cpp gtest
cp $1/test/test_overload_callback.cpp gtest
cp $1/test/test_record.cpp gtest
cp $1/test/test_resampler.cpp gtest
cp $1/test/test_ring_array.cpp gtest
cp $1/test/test_sanity.cpp gtest
cp $1/test/test_tone.cpp gtest
cp $1/test/test_utils.cpp gtest

if [ -d $1/.git ]; then
  rev=$(cd $1 && git rev-parse --verify HEAD)
  date=$(cd $1 && git show -s --format=%ci HEAD)
  dirty=$(cd $1 && git diff-index --name-only HEAD)
fi

if [ -n "$rev" ]; then
  version=$rev
  if [ -n "$dirty" ]; then
    version=$version-dirty
    echo "WARNING: updating from a dirty git repository."
  fi
  sed -i.bak -e "/The git commit ID used was/ s/[0-9a-f]\{40\}\(-dirty\)\{0,1\} .\{1,100\}/$version ($date)/" README_MOZILLA
  rm README_MOZILLA.bak
else
  echo "Remember to update README_MOZILLA with the version details."
fi

echo "Applying disable-assert.patch on top of $rev"
patch -p3 < disable-assert.patch

echo "Applying prefer-pulse-rust.patch on top of $rev"
patch -p3 < prefer-pulse-rust.patch
