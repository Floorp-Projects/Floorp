# Configure Java for Android
If you are building Android via the command line (which includes our recommended pre-push hooks), you will need to configure Java. If not, you can just use Android Studio and do not need to read further.

Notes:
- this guide is written using macOS and will need to be updated to additionally support Linux and Windows.
- if you find a better way to do configure Java, let us know!

## Background
At the time of writing, Android development seems to work best when using Java 8. By default `brew` and other package managers will install more recent versions that may throw errors during development.

Caveat: Robolectric warns that Java 9 is needed for certain functionality. This seems inconsistent with the rest of the Android ecosystem, however.

## Methods
### Method #1: configure Java 8 from Android Studio
Do the following:
* Download and install Android Studio
* Add the following line to your `~/.zshrc` or equivalent shell startup file: `export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"`
  * Update the path if you installed Android Studio to a non-standard location

That's it! To verify correctness, open a new shell, navigate to a directory with Android source code, and type `./gradlew tasks`.

This works because macOS comes with a `/usr/bin/java` stub which will defer the location of the JDK to the value in the `JAVA_HOME` environment variable.

### Method #2: install Java 11 from Homebrew
You can install Java from [Homebrew]([url](https://brew.sh/)) using the command below.

```bash
brew install java11
```

### Method #3: install from website
TODO

## Troubleshooting
### Get Java version
To see what version of Java you have installed, run:
```sh
java -version
```

If you have Java 8 configured, you'll see output like:
```
openjdk version "1.8.0_242-release"
OpenJDK Runtime Environment (build 1.8.0_242-release-1644-b3-6222593)
OpenJDK 64-Bit Server VM (build 25.242-b3-6222593, mixed mode)
```

### Error running pre-push hooks
If you encounter this error, particularly when running our recommended pre-push hooks:
```
Could not initialize class org.codehaus.groovy.runtime.InvokerHelper
```

You may be on Java 14: consider installing Java 8 (see above). If preferred, other users have found installing any version of Java, 13 or lower, appears to resolve the issue:

Steps to downgrade Java Version on Mac with Brew:
1. Install Homebrew (https://brew.sh/)
2. run ```brew update```
3. To uninstall your current java version, run ```sudo rm -fr
   /Library/Java/JavaVirtualMachines/<jdk-version>```
4. run ```brew tap homebrew/cask-versions```
5. run ```brew search java```
6. If you see java11, then run ```brew install java11```
7. Verify java-version by running ```java -version```
