#! /bin/sh
#
# parse the gtest results file this replaces a sed script which produced
# the identical output. This new script is now independent of new unknown
# labels being introduced in future revisions of gtests.

#this function extracts the appropriate value from
#  <testcase label="value1" label2="value2" label3="value3" />
# which value is selected from the label , which is specified
# as the 2nd parameter. The line to parse is the first parameter.
getvalue()
{
  pattern1='*'${2}'="'
  pattern2='"*'
  front=${1#${pattern1}}
  if [[ "${front}" != "${1}" ]]; then
    val=${front%%${pattern2}}
    # as we output the result, restore any quotes that may have
    # been in the original test names.
    echo ${val//&quot;/\"}
  fi
}

parse()
{
  while read line
  do
    if [[ "${line}" =~ "<testcase " ]]; then
      name=$(getvalue "${line}" "name")
      value=$(getvalue "${line}" "value_param")
      stat=$(getvalue "${line}" "status")
      class=$(getvalue "${line}" "classname")
      echo "${stat} '${class}: $(echo ${name} ${value})'"
    fi
  done
}

# if no arguments, just take standard in, if arguments, take the args as
# files and cat them together to parse
if [ $# -eq 0 ]; then
  parse
else
  cat "$@" | parse
fi
