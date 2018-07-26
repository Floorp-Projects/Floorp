# [Android Components](../../../README.md) > Support > Base

Base or core component containing building blocks and interfaces for other components.

Usually this component never needs to be added to application projects manually. Other components may have a transitive dependency on some of the classes and interfaces in this component.

## Usage

### Setting up the dependency

Use gradle to download the library from JCenter:

```Groovy
implementation "org.mozilla.components:base:{latest-version}
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
