# Configure Java for Android
If you are building Android via the command line (which includes our recommended pre-push hooks), you will need to configure Java. If not, you can just use Android Studio and do not need to read further.

Notes:
- this guide is written using macOS and will need to be updated to additionally support Linux and Windows.
- if you find a better way to do configure Java, let us know!

## Background
At the time of writing, Android development seems to work best when using Java 17. By default `brew` and other package managers will install more recent versions that may throw errors during development.

## Methods
### Method #1: configure Java 17 from Android Studio
Do the following:
* Download and install Android Studio
* Add the following line to your `~/.zshrc` or equivalent shell startup file: `export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"`
  * Update the path if you installed Android Studio to a non-standard location

That's it! To verify correctness, open a new shell, navigate to a directory with Android source code, and type `./gradlew tasks`.

This works because macOS comes with a `/usr/bin/java` stub which will defer the location of the JDK to the value in the `JAVA_HOME` environment variable.

### Method #2: install Java 17 from Homebrew
You can install Java from [Homebrew]([url](https://brew.sh/)) using the command below.

```bash
brew install openjdk@17
```

### Method #3: install from website
See https://docs.oracle.com/en/java/javase/17/install/installation-jdk-macos.html

## Troubleshooting
### Get Java version
To see what version of Java you have installed, run:
```sh
java -version
```

If you have Java 17 configured, you'll see output like:
```
openjdk 17.0.7 2023-04-18
OpenJDK Runtime Environment Homebrew (build 17.0.7+0)
OpenJDK 64-Bit Server VM Homebrew (build 17.0.7+0, mixed mode, sharing)
```
