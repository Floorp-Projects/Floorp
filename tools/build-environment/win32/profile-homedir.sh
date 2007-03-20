#!/bin/sh

if test -n "$USERPROFILE" -a "$HOME" = "/home/$LOGNAME"; then
    HOME=$(cd "$USERPROFILE" && pwd)
fi
export HOME

if test -f "$HOME/.bash_profile"; then
    . "$HOME/.bash_profile"
fi
