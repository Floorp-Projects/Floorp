# Kotlin guide
This guide is designed to help you get started with [Kotlin], the language we write most of our code in, and to introduce you to some intermediate topics.
- [Getting Started](#getting-started)
- [Staying Informed](#staying-informed)
- [Intermediate Topics](#intermediate-topics)

## Getting Started
There are multiple ways to get started with Kotlin, depending on how you like to learn. If you like:
- Learning while coding, try the [Kotlin Koans] online or in the IDE. Kotlin Koans are a series of exercises to get you familiar with the Kotlin syntax. Each exercise is created as a failing unit test and your job is to make it pass.
  - If you do it in the IDE, we recommend using [IntelliJ Community Edition] with the [Kotlin Edu plugin]: educational plugins may not work in Android Studio yet, but the Kotlin Edu plugin has a tighter integration than using the Koans on their own.
  - If you want to learn on the go, there's a good [Kotlin Koans Android app].
- Reading documentation, the official Kotlin docs are good: start at [Basic Syntax] and work your way through the pages in the side bar
- Reading books, try [Kotlin in Action]. If you're an SF employee, we have this on the bookshelf, as well as [Kotlin Programming: The Big Nerd Ranch Guide].
- Comparing Swift & Kotlin: [Swift is like Kotlin](http://nilhcem.com/swift-is-like-kotlin/) This site provides a ton of examples of how Swift code looks in Kotlin. If you're comfortable with Swift (or moving from iOS development) this is a great resource for you!

Once you've gained some basic familiarity with the language, you can gain more experience by:
- Watching videos and conference talks on YouTube.
- [Convert Java to Kotlin with the auto-converter][convert]and change the resulting code into idiomatic Kotlin
- Doing coding exercises on a site like [exercism.io](https://exercism.io/)
- Working on Mozilla code ;)

You should also take a look at the official style guides:
- [Jetbrains](https://kotlinlang.org/docs/reference/coding-conventions.html)
- [Android](https://android.github.io/kotlin-guides/)

## Staying Informed
If you like staying up-to-date on the latest Kotlin topics, members of our team recommend:
- Reading blogs
  - [Official Kotlin blog](https://blog.jetbrains.com/kotlin) (also has a newsletter)
- Subscribing to weekly newsletters
  - [Kotlin weekly](http://kotlinweekly.net/)
- Visiting conferences (or watching their recordings)
  - [KotlinConf](https://kotlinconf.com/)
- Visiting local meetups

## Intermediate Topics
You can see what the Kotlin compiler is doing under the hood (e.g. is this lambda performant? What does it keep a reference to?) by using "Show Kotlin Bytecode" and then "Decompile" back to Java.

Writing Domain Specific Languages (DSLs) is a neat feature in Kotlin: there's a chapter near the end of Kotlin in Action if you want to read about it.

### Coroutines
The official documentation on coroutines is really good:
- [kotlinx.coroutines by example](https://github.com/Kotlin/kotlinx.coroutines/blob/master/coroutines-guide.md)
- [UI programming with coroutines](https://github.com/Kotlin/kotlinx.coroutines/blob/master/ui/coroutines-guide-ui.md) (read the first guide first)

If you'd prefer to get started with videos, [this KotlinConf 2017 talk](https://www.youtube.com/watch?v=_hfBv0a09Jc) is a great place to get started.

[Kotlin]: https://kotlinlang.org/
[Basic Syntax]: https://kotlinlang.org/docs/reference/basic-syntax.html
[Kotlin Koans]: https://kotlinlang.org/docs/tutorials/koans.html
[course]: https://www.udacity.com/course/kotlin-for-android-developers--ud888
[Kotlin in Action]: https://www.manning.com/books/kotlin-in-action
[convert]: https://www.jetbrains.com/help/idea/converting-a-java-file-to-kotlin-file.html
[IntelliJ Community Edition]: https://www.jetbrains.com/idea/download/
[Kotlin Edu plugin]: https://www.jetbrains.com/education/kotlin-edu/
[Kotlin Programming: The Big Nerd Ranch Guide]: https://www.bignerdranch.com/books/kotlin-programming/
[Kotlin Koans Android app]: https://play.google.com/store/apps/details?id=me.vickychijwani.kotlinkoans
