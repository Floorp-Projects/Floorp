#!/usr/bin/expect -f

# From: https://github.com/mindrunner/docker-android-sdk/blob/master/tools/

set timeout 1800
set cmd [lindex $argv 0]
set licenses [lindex $argv 1]

spawn {*}$cmd
expect {
  "Do you accept the license '*'*" {
        exp_send "y\r"
        exp_continue
  }
  eof
}
