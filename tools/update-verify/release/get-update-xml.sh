#!/bin/bash

update_xml_url="${1}"
patch_types="${2}"
update_xml="$(mktemp -t update.xml.XXXXXXXXXX)"
update_xml_headers="$(mktemp -t update.xml.headers.XXXXXXXXXX)"
update_xml_debug="$(mktemp -t update.xml.debug.XXXXXXXXXX)"
curl --retry 50 --retry-max-time 300 -s --show-error -D "${update_xml_headers}" -L -v -H "Cache-Control: max-stale=0" "${update_xml_url}" > "${update_xml}" 2>"${update_xml_debug}"
update_xml_curl_exit_code=$?
if [ "${update_xml_curl_exit_code}" == 0 ]
then
    update_xml_actual_url="$(sed -e "s/$(printf '\r')//" -n -e 's/^Location: //p' "${update_xml_headers}" | tail -1)"
    [ -n "${update_xml_actual_url}" ] && update_xml_url_with_redirects="${update_xml_url} => ${update_xml_actual_url}" || update_xml_url_with_redirects="${update_xml_url}"
    echo "$(date):  Downloaded update.xml file from ${update_xml_url_with_redirects}" > "$(mktemp -t log.XXXXXXXXXX)"
    for patch_type in ${patch_types//,/ }
    do  
        mar_url_and_size="$(sed -e 's/\&amp;/\&/g' -n -e 's/.*<patch .*type="'"${patch_type}"'".* URL="\([^"]*\)".*size="\([^"]*\)".*/\1 \2/p' "${update_xml}" | tail -1)"
        if [ -z "${mar_url_and_size}" ]
        then
            echo "$(date):  FAILURE: No patch type '${patch_type}' found in update.xml from ${update_xml_url_with_redirects}" > "$(mktemp -t log.XXXXXXXXXX)"
            echo "PATCH_TYPE_MISSING ${update_xml_url} ${patch_type} ${update_xml} ${update_xml_headers} ${update_xml_debug} ${update_xml_actual_url}" > "$(mktemp -t failure.XXXXXXXXXX)"
        else
            echo "$(date):  Mar url and file size for patch type '${patch_type}' extracted from ${update_xml_url_with_redirects} (${mar_url_and_size})" > "$(mktemp -t log.XXXXXXXXXX)"
            echo "${mar_url_and_size} ${update_xml_url} ${patch_type} ${update_xml_actual_url}" > "$(mktemp -t update_xml_to_mar.XXXXXXXXXX)"
        fi
    done
else
    if [ -z "${update_xml_actual_url}" ]
    then
        echo "$(date):  FAILURE: Could not retrieve update.xml from ${update_xml_url} for patch type(s) '${patch_types}'" > "$(mktemp -t log.XXXXXXXXXX)"
        echo "UPDATE_XML_UNAVAILABLE ${update_xml_url} ${update_xml} ${update_xml_headers} ${update_xml_debug} ${update_xml_curl_exit_code}" > "$(mktemp -t failure.XXXXXXXXXX)"
    else
        echo "$(date):  FAILURE: update.xml from ${update_xml_url} redirected to ${update_xml_actual_url} but could not retrieve update.xml from here" > "$(mktemp -t log.XXXXXXXXXX)"
        echo "UPDATE_XML_REDIRECT_FAILED ${update_xml_url} ${update_xml_actual_url} ${update_xml} ${update_xml_headers} ${update_xml_debug} ${update_xml_curl_exit_code}" > "$(mktemp -t failure.XXXXXXXXXX)"
    fi
fi
