# Android Versioning

This repository uses the same `version.txt` to define the version for Android Components, Focus, and Fenix. The following formatting corresponds to release branches
on the release track.

## Format

XXX.0 (where XXX is defined as the current desktop release #)

*Please note: the initial release is `major.minor`, but the following releases will be `major.minor.patch`*

* The first and second digit will be directly tied to the associated desktop release
* The third digit will be reserved for Android-only patch releases

### Example
|                                | Desktop               | Android                        |
| ------------------------------ | --------------------- | ------------------------------ |
| Fx XXX initial release         | XXX.0                 | XXX.0                          |
| Desktop + Android dot release  | XXX.0.1               | XXX.1.0                        |
| Android-only patch             | <sub>No change</sub>  | XXX.1.1                        |
| Desktop-only dot release       | XXX.0.2               | <sub>XXX.2.0 is skipped </sub> |
| Android-only dot release       | <sub>No change</sub>  | XXX.2.1                        |
| Desktop + Android dot release  | XXX.0.3               | XXX.3.0                        |
| Desktop-only dot release       | XXX.0.4               | <sub>XXX.4.0 is skipped </sub> |
| Desktop + Android dot release  | XXX.0.5               | XXX.5.0                        |
