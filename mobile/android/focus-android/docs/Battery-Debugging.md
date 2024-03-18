# Battery Debugging

Android Performance Profiling can be quite tricky in spite of the wealth of good tools available. This doc is to explain how to track down battery and performance issues.

## Battery Historian

Whenever battery issues arise on Android, the first Google suggestion is to use the Battery Historian tool to parse an Android bug report and show what caused the battery drain.

While this is a good option for most Android apps, it proves next to useless for analyzing most battery issues in Focus and Klar. This is because Focus and Klar uses a foreground service that remains running as long as your web pages are open. The service is required to keep the app in the foreground so data can remain in memory rather than being persisted to disk. This keeps the app from being killed by the OS and sending all your precious web session and page data to the garbage collector.

Battery Historian starts from the premise that most battery problems are caused by excessive wake locks. It helps the programmer by showing wake locks, when the WiFi or cellular radio is being kept awake, and the rate of battery drain at different times. Focus and Klar do not require wake locks and they wouldn't be interesting, since the apps remain in the foreground most of the time the app is running.

Analyzing Battery Historian results can still prove interesting. [Here are the instructions](https://developer.android.com/studio/profile/battery-historian) to try.

## Android Studio Profiler

If Battery Historian does not point at the radio or screen being kept alive as the issue, the next thing is to look for excessive CPU activity. There are several tools to look into this, but the easiest to get started is the profiler built into Android Studio.

Open the project in Android Studio. If there is no Android Profiler tab on the bottom of the screen, open it from View -> Tool Windows -> Android Profiler.

Make sure you're running a debug version of the app and choose the device and debuggable process from the dropdowns at the top of the Profiler.

Click the CPU graph to get additional CPU detail. Choose instrumented to increase the resolution of the data. If the battery drain is happening while the phone is idle, you can easily check to find out what is causing the CPU activity, since there shouldn't be much at all.

[Here's more](https://developer.android.com/studio/profile/android-profiler) on how to use the Android Profiler in general. [Here's more on the CPU Profiler](https://developer.android.com/studio/profile/cpu-profiler) specifically.

## Systrace

Systrace does not add that much to what Android Studio's Profiler already provides and the interface is a clunky webpage. You can still try it if you like. [Here are the instructions](https://developer.android.com/studio/command-line/systrace).

## Traceview

While you can get a lot of useful and interesting information from the Android Profiler and Systrace, it's helpful to get full call stacks. The easiest way to do this is to use the older Android profiling tool, Traceview. You can select from the code when to start and stop tracing with "Debug.startMethodTracing("name")" and "Debug.stopMethodTracing()"

When you run the code, it'll generate a trace file in the files folder on the SDcard. You can read the trace file either by double-clicking it in Android Studio's Device File Explorer or even better by opening it in Android Device Monitor. You can start Android Device Monitor in recent Android Studio versions by running $ANDROID_SDK/tools/monitor.

[Here's more detail on using TraceView](https://developer.android.com/studio/profile/traceview).
