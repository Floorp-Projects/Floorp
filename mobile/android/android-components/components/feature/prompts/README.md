# [Android Components](../../../README.md) > Feature > Prompts

A feature that will subscribe to the selected session and will handle all the common prompt dialogs from web content like select, option and menu html elements.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-prompts:{latest-version}"
```

### PromptFeature

  ```kotlin
  val promptFeature = PromptFeature(sessionManager, fragmentManager)

  //It will start listing for new prompt requests for web content.
  promptFeature.start()

  //It will stop listing for future prompt requests for web content.
  promptFeature.stop()

  ```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
