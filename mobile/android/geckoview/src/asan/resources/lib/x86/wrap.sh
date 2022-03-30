#!/system/bin/sh
# shellcheck shell=ksh

# call getprop before setting LD_PRELOAD
os_version=$(getprop ro.build.version.sdk)

# These options mirror those in mozglue/build/AsanOptions.cpp
# except for fast_unwind_* which are only needed on Android
options=(
  allow_user_segv_handler=1
  alloc_dealloc_mismatch=0
  detect_leaks=0
  fast_unwind_on_check=1
  fast_unwind_on_fatal=1
  max_free_fill_size=268435456
  max_malloc_fill_size=268435456
  malloc_fill_byte=228
  free_fill_byte=229
  handle_sigill=1
  allocator_may_return_null=1
)
if [ -e "/data/local/tmp/asan.options.gecko" ]; then
  options+=("$(tr -d '\n' < /data/local/tmp/asan.options.gecko)")
fi

# : is the usual separator for ASAN options
# save and reset IFS so it doesn't interfere with later commands
old_ifs="$IFS"
IFS=:
ASAN_OPTIONS="${options[*]}"
export ASAN_OPTIONS
IFS="$old_ifs"

LIB_PATH="$(cd "$(dirname "$0")" && pwd)"
LD_PRELOAD="$(ls "$LIB_PATH"/libclang_rt.asan-*-android.so)"
export LD_PRELOAD

cmd="$1"
shift

# enable debugging
# https://developer.android.com/ndk/guides/wrap-script#debugging_when_using_wrapsh
# note that wrap.sh is not supported before android 8.1 (API 27)
if [ "$os_version" -eq "27" ]; then
  args=("-Xrunjdwp:transport=dt_android_adb,suspend=n,server=y" -Xcompiler-option --debuggable)
elif [ "$os_version" -eq "28" ]; then
  args=(-XjdwpProvider:adbconnection "-XjdwpOptions:suspend=n,server=y" -Xcompiler-option --debuggable)
else
  args=(-XjdwpProvider:adbconnection "-XjdwpOptions:suspend=n,server=y")
fi

exec "$cmd" "${args[@]}" "$@"
