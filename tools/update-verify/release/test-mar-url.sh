#!/bin/bash
mar_url="${1}"
mar_required_size="${2}"

mar_headers_file="$(mktemp -t mar_headers.XXXXXXXXXX)"
mar_headers_debug_file="$(mktemp -t mar_headers_debug.XXXXXXXXXX)"
curl --retry 50 --retry-max-time 300 -s -i -r 0-2 -L -v "${mar_url}" > "${mar_headers_file}" 2>"${mar_headers_debug_file}"
mar_file_curl_exit_code=$?

# Bug 894368 - HTTP 408's are not handled by the "curl --retry" mechanism; in this case retry in bash
attempts=1
while [ "$((++attempts))" -lt 50 ] && grep 'HTTP/1\.1 408 Request Timeout' "${mar_headers_file}" &>/dev/null
do
    sleep 1
    curl --retry 50 --retry-max-time 300 -s -i -r 0-2 -L -v "${mar_url}" > "${mar_headers_file}" 2>"${mar_headers_debug_file}"
    mar_file_curl_exit_code=$?
done

# check file size matches what was written in update.xml
# strip out dos line returns from header if they occur
# note: below, using $(printf '\r') for Darwin compatibility, rather than simple '\r'
# (i.e. shell interprets '\r' rather than sed interpretting '\r')
mar_actual_size="$(sed -e "s/$(printf '\r')//" -n -e 's/^Content-Range: bytes 0-2\///ip' "${mar_headers_file}" | tail -1)"
mar_actual_url="$(sed -e "s/$(printf '\r')//" -n -e 's/^Location: //p' "${mar_headers_file}" | tail -1)"
# note: below, sed -n '/^HTTP\//p' acts as grep '^HTTP/', but requires less overhead as sed already running
http_response_code="$(sed -e "s/$(printf '\r')//" -n -e '/^HTTP\//p' "${mar_headers_file}" | tail -1)"

[ -n "${mar_actual_url}" ] && mar_url_with_redirects="${mar_url} => ${mar_actual_url}" || mar_url_with_redirects="${mar_url}"

if [ "${mar_actual_size}" == "${mar_required_size}" ]
then
    echo "$(date):  Mar file ${mar_url_with_redirects} available with correct size (${mar_actual_size} bytes)" > "$(mktemp -t log.XXXXXXXXXX)"
elif [ -z "${mar_actual_size}" ]
then
    echo "$(date):  FAILURE: Could not retrieve http header for mar file from ${mar_url}" > "$(mktemp -t log.XXXXXXXXXX)"
    echo "NO_MAR_FILE ${mar_url} ${mar_headers_file} ${mar_headers_debug_file} ${mar_file_curl_exit_code} ${mar_actual_url}" > "$(mktemp -t failure.XXXXXXXXXX)"
    # If we get a response code (i.e. not an empty string), it better contain "206 Partial Content" or we should report on it.
    # If response code is empty, this should be caught by a different block to this one (e.g. "could not retrieve http header").
elif [ -n "${http_response_code}" ] && [ "${http_response_code}" == "${http_response_code/206 Partial Content/}" ]
then
    echo "$(date):  FAILURE: received a '${http_response_code}' response for mar file from ${mar_url} (expected HTTP 206 Partial Content)" > "$(mktemp -t log.XXXXXXXXXX)"
    echo "BAD_HTTP_RESPONSE_CODE_FOR_MAR ${mar_url} ${mar_headers_file} ${mar_headers_debug_file} ${mar_file_curl_exit_code} ${mar_actual_url}" > "$(mktemp -t failure.XXXXXXXXXX)"
else
    echo "$(date):  FAILURE: Mar file incorrect size - should be ${mar_required_size} bytes, but is ${mar_actual_size} bytes - ${mar_url_with_redirects}" > "$(mktemp -t log.XXXXXXXXXX)"
    echo "MAR_FILE_WRONG_SIZE ${mar_url} ${mar_required_size} ${mar_actual_size} ${mar_headers_file} ${mar_headers_debug_file} ${mar_file_curl_exit_code} ${mar_actual_url}" > "$(mktemp -t failure.XXXXXXXXXX)"
fi
