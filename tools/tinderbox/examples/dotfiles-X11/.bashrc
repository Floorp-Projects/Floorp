# .bashrc
# vim:ts=8:sw=8:noet:

# User specific aliases and functions

# Source global definitions
if [ -f /etc/bashrc ]; then
	. /etc/bashrc
fi

set -o noclobber

export EDITOR=/usr/bin/vim
export VISUAL=$EDITOR			# set default editor for mail
export PAGER="less -Cemf"	# make some things start at top of screen.

export LC_COLLATE=C

export PS1="\u@\h `uname -s` (\$?) \w \\\$ "
ulimit -S -c 0  # limit core dump size (use 'ulimit -a' to check status)
alias coreok="ulimit -c unlimited"

alias rm="/bin/rm -i"		# safe deletion, always ask
alias mv="/bin/mv -i"
alias cp="/bin/cp -i"
alias rename="rename -i"

alias c="clear"

alias vi=vim

alias pico="pico -z"
alias pine="pine -z"
alias ftp="ncftp"
alias md="mkdir"
alias rd="rmdir"

alias less="less -Cemf"
alias more="less -Cemf"

alias ls="/bin/ls --color=tty -FA"
alias lsl="lsa -lg"
