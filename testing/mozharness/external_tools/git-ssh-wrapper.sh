#!/bin/sh
# From http://www.reddit.com/r/git/comments/hdn1a/howto_using_the_git_ssh_variable_for_private_keys/

# In the example, this was
#    if [ -e "$GIT_SSH_KEY" ]; then
# However, that broke on tilde expansion.
# Let's just assume if GIT_SSH_KEY is set, we want to use it.
if [ "x$GIT_SSH_KEY" != "x" ]; then
    exec ssh -o IdentityFile="$GIT_SSH_KEY" -o ServerAliveInterval=600 "$@"
else
    exec ssh -o ServerAliveInterval=600 "$@"
fi
