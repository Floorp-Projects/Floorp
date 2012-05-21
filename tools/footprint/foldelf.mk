# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

foldelf: foldelf.cpp
	$(CXX) -O -o foldelf foldelf.cpp

clean:
	rm -f *.o test *.test foldelf *~

