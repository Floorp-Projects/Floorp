# Retrieving crash reports from the application
* Open Firefox
* Tap on the `3 dot menu`
* Tap `Settings`
* Scroll to the bottom of Settings
* Tap `About Firefox`
* Tap `Crashes`
* Tap on the Socorro link
* Copy and paste that address into a new [Github Issue: 🐞 Bug report](https://github.com/mozilla-mobile/fenix/issues/new/choose) or an existing issue
* If you have many crash reports it can be helpful to include several of the recent crash URLs


![Screenshot showing where to find the settings and About Firefox elements of the app. The bulleted steps above contain the same information.](https://user-images.githubusercontent.com/250273/84347868-7bc9e980-ab68-11ea-990d-7284968c458a.png)
![Screenshot showing where to find the Crashes item in About Firefox and the Socorro link for the crash. ](https://user-images.githubusercontent.com/250273/84347924-a156f300-ab68-11ea-9d02-c984a030249f.png)

# Using adb logcat to get crash information

Please use the above directions as a first way to get info. Use the following directions if your crash does not show up in the crash window of Firefox or if the crash prevents you from accessing the settings of Firefox.

To get information about a crash you will need an Android device that reproduces a crash, a computer running Windows, macOS or Linux and a USB cable that connects your device to your computer.

## Configuring your phone
* Enable Developer Mode
  * On stock Android open the Android Settings and use the search at the top to find `build number`
  * Tap the build number 7 times
  * For other devices use your favorite search engine or YouTube to find steps for your device.
* Enable Android USB debugging
  * On stock Android Open the Android Settings and use the search at the top to find `USB debugging` and enable it
* Connect your device to the computer using a USB cable

## Downloading the Android SDK Platform tools
* Download the [Android SDK Platform tools](https://developer.android.com/studio/releases/platform-tools) for your operating system
* Use your operating system tools to extract the zip file that was downloaded

## Checking that adb can see your phone and is authorized
* Connect your device to the computer using a USB cable
* Open a command prompt or terminal and change to the directory to the platform tools directory that was extracted
* On Windows run the command `adb devices` on macOS and Linux run `./adb devices`
* If it returns unauthorized you will need to authorize the phone to connect to the computer by accepting the connection dialog on the phone

## Reproducing the crash
* Connect your device to the computer using a USB cable
* Open a command prompt or terminal and change to the directory to the platform tools directory that was extracted
* On Windows run `adb logcat -v time` on macOS and Linux run `./adb logcat -v time`
* Reproduce the crash
* Submit the crash report(s)
* Unplug the device
* Copy all the information in the terminal
* Paste the information into a Gist https://gist.github.com/ and save logcat information
* Add the Gist URL to the issue for the crash

## Optional Cleanup
* It is recommended to disable USB debugging once you are done collecting this information
* You can remove the Android SDK Platform tools by deleting the folders and zip file
