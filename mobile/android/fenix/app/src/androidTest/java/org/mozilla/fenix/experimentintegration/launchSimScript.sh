#!/usr/bin/env bash
set -e

echo "Waiting emulator is ready..."
~/Library/Android/sdk/emulator/emulator -avd Pixel_3_API_28 -wipe-data -no-boot-anim -screen no-touch &

bootanim=""
failcounter=0
timeout_in_sec=360

until [[ "$bootanim" =~ "stopped" ]]; do
  bootanim=`~/Library/Android/sdk/platform-tools/adb -e shell getprop init.svc.bootanim 2>&1 &`
  if [[ "$bootanim" =~ "device not found" || "$bootanim" =~ "device offline"
    || "$bootanim" =~ "running" ]]; then
    let "failcounter += 1"
    echo "Waiting for emulator to start"
    if [[ $failcounter -gt timeout_in_sec ]]; then
      echo "Timeout ($timeout_in_sec seconds) reached; failed to start emulator"
      exit 1
    fi
  fi
  sleep 1
done

echo "Emulator is ready"
sleep 10
