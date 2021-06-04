# Remote Settings Initial Data

In order to reduce the amount of data to be downloaded on first synchronization,
a JSON dump from the records present on the remote server can be shipped with the
release.

The dumps in this directory will NOT automaticaly be kept in sync with remote.
This is useful for collections that benefit from a default iniital state but
require dynamism beyond that - e.g. DoH regional configurations. For dumps
that should automatially be kept in sync with remote, use ../dumps/.

Dumps from dumps/ and static-dumps/ are packaged into the same resource path,
thus looking the same to the client code and also implying that filenames must
be unique between the two directories.
