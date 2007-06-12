#!/usr/bin/expect 

spawn hdiutil attach $argv

expect {
"byte" {send "G"; exp_continue}
"Y/N"  {send "Y\r"; exp_continue}
}

