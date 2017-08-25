#!/usr/bin/expect -f
# ignore-license

set timeout 1800
set cmd [lindex $argv 0]
set licenses [lindex $argv 1]

spawn {*}$cmd
expect {
  "Accept? (y/N):*" {
        exp_send "y\r"
        exp_continue
  }
  eof
}