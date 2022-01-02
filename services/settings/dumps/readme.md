# Remote Settings Initial Data

In order to reduce the amount of data to be downloaded on first synchronization,
a JSON dump from the records present on the remote server can be shipped with the
release.

A bot will update the files automatically. For collections that should not be kept
in sync, put the JSON dumps in ../static-dumps/ instead.

Dumps from dumps/ and static-dumps/ are packaged into the same resource path,
thus looking the same to the client code and also implying that filenames must
be unique between the two directories.
