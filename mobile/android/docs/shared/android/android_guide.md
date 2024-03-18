# Android guide
This guide is designed to help you get started on Android and introduce you to some intermediate topics.

- [Getting started](#getting-started)
- [Staying informed](#staying-informed)
- [Intermediate topics](#intermediate-topics)

## Getting started
Assuming you already have some coding experience, Google's official [Developing Android Apps udacity course][udacity course] comes highly recommended by our team. After completing your first few lessons, you can try to working on our code base, continuing the course as necessary.

Once you have a little experience, the beginner's guides in the [official Android developer docs][devdocs] teach popular topics pretty well and are a good reference. However, they aren't very good at telling you where to go next (which is why we recommended the course). Note: the documentation on less popular topics are often missing information.

There are also many good Android tips and conference talks on YouTube.

You may wish to see [our guide for properly configuring Java](configure_java.md) to properly set up command line tools.

## Staying informed
If you like staying up-to-date on the latest Android topics, members of our team recommend:
- Reading blogs
  - [Official Android developer blog](https://android-developers.googleblog.com/) (also has a newsletter)
- Subscribing to weekly newsletters
  - [Android weekly](https://androidweekly.net/)
  - [Jetpack Compose Newsletter by Commonsware](https://jetc.dev/)
- Listening to podcasts
  - [Fragmented](http://fragmentedpodcast.com/)
  - [Android Developers Backstage](https://androidbackstage.blogspot.com/)
- Visiting conferences (or watching their recordings)
  - Google IO
  - Android Developer Summit
  - Droidcon
- Visiting local meetups

## Intermediate topics
Comfortable with the basics? Here are a few of our favorite resources to get started on intermediate topics.
- [Android style tips](http://blog.danlew.net/2014/11/19/styles-on-android/) (circa 2014)
- [Our Accessibility guide](accessibility_guide.md)
- [WorkManager FAQ](workmanager_faq.md)

### Navigating the Android source code
Oftentimes, it's easier to solve a problem when you understand how your code is interacting with the Android framework: you can do this by reading the Android source code! You can do plain-text searches on the Android source code at http://androidxref.com/ (choosing the latest API level and searching in the "frameworks" is usually sufficient).

You can also navigate the Android source code in Android Studio, after downloading the sources:
- Click a framework method and use "go to definition"
- Step into framework methods via the debugger
- Set breakpoints in framework files to stop whenever the framework calls that method

[udacity course]: https://www.udacity.com/course/new-android-fundamentals--ud851
[devdocs]: https://developer.android.com/docs/
