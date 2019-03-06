# this library works like a wrapper around wget, to allow downloads to be cached
# so that if later the same url is retrieved, the entry from the cache will be
# returned.

pushd `dirname $0` &>/dev/null
cache_dir="$(pwd)/cache"
popd &>/dev/null

# to clear the entire cache, recommended at beginning and end of scripts that call it
clear_cache () {
    rm -rf "${cache_dir}"
}

# creates an empty cache, should be called once before downloading anything
function create_cache () {
    mkdir "${cache_dir}"
    touch "${cache_dir}/urls.list"
}

# download method - you pass a filename to save the file under, and the url to call
cached_download () {
    local output_file="${1}"
    local url="${2}"

    if fgrep -x "${url}" "${cache_dir}/urls.list" >/dev/null; then
        echo "Retrieving '${url}' from cache..."
        local line_number="$(fgrep -nx  "${url}" "${cache_dir}/urls.list" | sed 's/:.*//')"
        cp "${cache_dir}/obj_$(printf "%05d\n" "${line_number}").cache" "${output_file}"
    else
        echo "Downloading '${url}' and placing in cache..."
        rm -f "${output_file}"
        $retry wget -O "${output_file}" --progress=dot:mega --server-response "${url}" 2>&1
        local exit_code=$?
        if [ "${exit_code}" == 0 ]; then
            echo "${url}" >> "${cache_dir}/urls.list"
            local line_number="$(fgrep -nx  "${url}" "${cache_dir}/urls.list" | sed 's/:.*//')"
            cp "${output_file}" "${cache_dir}/obj_$(printf "%05d\n" "${line_number}").cache"
        else
            return "${exit_code}"
        fi
    fi
}
