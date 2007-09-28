#!/bin/bash -e

delim=,

while getopts "d:" optname;
  do
  case $optname in
      d) delim=$OPTARG;;
  esac
done

if [[ $OPTIND -gt 1 ]]; then
    shift 2
fi


if [[ -n "$4" ]]; then
    for a in $1; do for b in $2; do for c in $3; do for d in $4; do echo $a$delim$b$delim$c$delim$d; done; done; done; done
elif [[ -n "$3" ]]; then
    for a in $1; do for b in $2; do for c in $3; do echo $a$delim$b$delim$c; done; done; done
elif [[ -n "$2" ]]; then
    for a in $1; do for b in $2; do echo $a$delim$b; done; done
elif [[ -n "$1" ]]; then
    for a in $1; do echo $a; done
else
    cat<<EOF 
usage: combo.sh [-d delim] list1 [list2 [list3 [list4]]]

output combinations of items in each list using delim as the delimiter

-d delim specifies the delimiter. The default is comma (,).

$combo.sh "a b" 
a
b

combo.ah "a b" "1 2" 
a,1 
a,2 
b,1 
b,2

combo.ah -d "-" "a b" "1 2" 
a-1 
a-2 
b-1 
b-2

EOF

fi
