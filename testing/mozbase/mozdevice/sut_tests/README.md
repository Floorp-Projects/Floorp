# SUT Agent tests

* In order to run these tests you need to have a phone running SUT Agent
connected.

* Make sure you can reach the device's TCP 20700 and 20701 ports. Doing
*adb forward tcp:20700 tcp:20700 && adb forward tcp:20701 tcp:20701* will
forward your localhost 20700 and 20701 ports to the ones on the device.

* *You might need some common tools like cp. Use this script to install them:
http://pastebin.mozilla.org/1724898*

* Make sure the SUTAgent on the device is running.

* Run: python runtests.py
