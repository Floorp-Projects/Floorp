# Firebase Cloud Messaging for WebPush

## Testing
If you want to test WebPush support for features like Send Tab or WebPush for web apps, then you need to follow the instructions below to enable this for your debug application:

1. Download the XML credential file from the [Fenix team's Google Drive](https://drive.google.com/file/d/1DwgmqJTSKOY9vMuW2OmRNDXpIY69thTI/view?usp=drive_link).
2. Place the file in the directory `fenix/app/src/debug/res/values/`.
    * This will be compiled into the `fenixDebug` variant.
3. Run the app and verify you receive log messages in `adb logcat` like the ones below:
    ```
    Received a new registration token from push service.
    Got new Firebase token.
    ```

## Components and code
The important components for push to work are the `feature-push` (specifically look at the [README](https://searchfox.org/mozilla-mobile/source/firefox-android/android-components/components/feature/push/README.md)) and `lib-push-firebase`.

For GeckoView, look at [`WebPushEngineIntegration.kt`](https://searchfox.org/mozilla-mobile/source/firefox-android/fenix/app/src/main/java/org/mozilla/fenix/push/WebPushEngineIntegration.kt) that connects the `AutoPushFeature` from `feature-push` to the `Engine`.

For Firefox Sync, it's the [`feature-accounts-push`](https://searchfox.org/mozilla-mobile/source/firefox-android/android-components/components/feature/accounts-push).

## Firefox Sync & Send Tab
If you are testing WebPush support with Firefox Sync, you should also expect to see this log shortly after signing into a Firefox Sync account:
```
Created a new subscription: [context data]
```

## FAQ

**Q. Why are credentials stored in Google Drive?**
  * Firebase services require a credential file, typically named `google-services.json`, which are retrieved from the Firebase console for all the applications in that project. Our requirements are somewhat unique, so we generate our own XML file from `google-services.json` and use that.
    According to [Firebase engineers](https://groups.google.com/forum/#!msg/firebase-talk/bamCgTDajkw/uVEJXjtiBwAJ), it should be safe to commit our `google-services.json` but we avoid doing that as there are other forks of Fenix that must not use these credentials. 
    For example, the Google I/O app commits [their release version](https://github.com/google/iosched/blob/b428d2be4bb96bd423e47cb709c906ce5d02150f/mobile/google-services.json) of the file to Github as well. 

**Q. What are the special requirements which mean we can't directly use `google-services.json`?**

  *  Following the 'Getting Started' Firebase instructions to initialize the service in Fenix would not work as we explicitly chose not to use the `com.google.gms.google-services` Gradle plugin as the Android service would be initialized with a [`ContentProvider`](https://firebase.blog/posts/2016/12/how-does-firebase-initialize-on-android) eagerly which reduces our control on how the application can start up in a performant way since the Browser startup sequence is quite unique.

**Q. Where do we get the `google-services.json` file from?**
  * Ask your friendly Release Management teammate if they can access the Cloud Messaging console at `console.firebase.google.com`.

**Q. How do I generate the XML file from `google-services.json`?**

  * The easiest way is to use [this clever web app](https://dandar3.github.io/android/google-services-json-to-xml.html) that does this for you.
  * The more official and tedious way, would be to do this manually by following [the instructions on the google services plugin](https://developers.google.com/android/guides/google-services-plugin#adding_the_json_file) site.
  * Note that the `google-services.json` file may have the credentials for multiple applications, so ensure you are copying the instructions for the correct project.
