# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

COUNTER=75
while [ $COUNTER -ge "1" ] 
do 
	COUNTER=$(eval expr $COUNTER - 1)
	echo $COUNTER
    	*/makepqg.exe -r -l 640 -g 160 || exit 1
done

