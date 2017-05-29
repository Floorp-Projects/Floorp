# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# a2po has problems with the folder format of Pontoon and creates resource folders
# like values-es-MX. Android expects values-es-rMX. This script tries to find those
# folders and fixes them.

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

SIZE_LIMIT=4194304

cd "$parent_path/../../app/build/outputs/apk/"

echo "Checking release APK file sizes ..."

for filename in *release*.apk; do
	let filesize=$(stat -f%z "$filename")

	if [[ ($filesize > $SIZE_LIMIT) ]]; then
		echo " * [TOOBIG] $filename ($filesize > $SIZE_LIMIT)";
		exit 27
	else
		echo " * [OKAY] $filename ($filesize <= $SIZE_LIMIT)";
	fi
done

