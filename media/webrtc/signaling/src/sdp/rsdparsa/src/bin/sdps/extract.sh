#!/bin/bash

grep '\"[ a-z]=[^=]*$' sdp_unittests.cpp  | grep -v 'ParseSdp(kVideoSdp' | grep -v 'kVideoWithRedAndUlpfec' | grep -v 'ASSERT_NE' | grep -v 'BASE64_DTLS_HELLO' | grep -v '^\/\/' | sed 's/ParseSdp(//g' | sed 's/^[[:space:]]*//' | sed 's/+ //' | sed 's/ \/\/.*$//' | sed 's/\;$//' | sed 's/)$//' | sed 's/, false//' | sed 's/" CRLF//' | sed 's/^\"//' | sed 's/\"$//' | sed 's/\\r\\n//' | gawk -v RS='(^|\n)v=' '/./ { print "v="$0 > NR".sdp" }'
