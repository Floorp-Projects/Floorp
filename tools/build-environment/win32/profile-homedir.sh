#!/bin/sh

if test -n "$USERPROFILE"; then
    HOME=$(cd "$USERPROFILE" && pwd)
fi
export HOME

if test -f "$HOME/.bash_profile"; then
    . "$HOME/.bash_profile"
fi
