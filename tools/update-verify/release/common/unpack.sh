#!/bin/bash

unpack_build () {
    unpack_platform="$1"
    dir_name="$2"
    pkg_file="$3"
    locale=$4
    unpack_jars=$5
    update_settings_string=$6
    # If provided, must be a directory containing `update-settings.ini` which
    # will be used instead of attempting to find this file in the unpacked
    # build. `update_settings_string` modifications will still be performed on
    # the file.
    local mac_update_settings_dir_override
    mac_update_settings_dir_override=$7
    local product
    product=$8

    if [ ! -f "$pkg_file" ]; then
      return 1
    fi 
    mkdir -p "$dir_name"
    pushd "$dir_name" > /dev/null || exit
    case $unpack_platform in
        # $unpack_platform is either
        # - a balrog platform name (from testing/mozharness/scripts/release/update-verify-config-creator.py)
        # - a simple platform name (from tools/update-verify/release/updates/verify.sh)
        mac|Darwin_*)
            os=$(uname)
            # How we unpack a dmg differs depending on which platform we're on.
            if [[ "$os" == "Darwin" ]]
            then
                cd ../
                echo "installing $pkg_file"
                ../common/unpack-diskimage.sh "$pkg_file" mnt "$dir_name"
            else
                7z x ../"$pkg_file" > /dev/null
                if [ "$(find . -mindepth 1 -maxdepth 1 | wc -l)" -ne 1 ]
                then
                    echo "Couldn't find .app package"
                    return 1
                fi
                unpack_dir=$(ls -1)
                unpack_dir=$(ls -d "${unpack_dir}")
                mv "${unpack_dir}"/*.app .
                rm -rf "${unpack_dir}"
                appdir=$(ls -1)
                appdir=$(ls -d ./*.app)
                if [ -d "${mac_update_settings_dir_override}" ]; then
                    cp "${mac_update_settings_dir_override}/update-settings.ini" "${appdir}/update-settings.ini"
                else
                    # The updater guesses the location of these files based on
                    # its own target architecture, not the mar. If we're not
                    # unpacking mac-on-mac, we need to copy them so it can find
                    # them. It's important to copy (and not move), because when
                    # we diff the installer vs updated build afterwards, the
                    # installer version will have them in their original place.
                    cp "${appdir}/Contents/Resources/update-settings.ini" "${appdir}/update-settings.ini"
                fi
                cp "${appdir}/Contents/Resources/precomplete" "${appdir}/precomplete"
            fi
            update_settings_file="${appdir}/update-settings.ini"
            ;;
        win32|WINNT_*)
            7z x ../"$pkg_file" > /dev/null
            if [ -d localized ]
            then
              mkdir bin/
              cp -rp nonlocalized/* bin/
              cp -rp localized/*    bin/
              rm -rf nonlocalized
              rm -rf localized
              if [ "$(find optional/ | wc -l)" -gt 1 ]
              then 
                cp -rp optional/*     bin/
                rm -rf optional
              fi
            elif [ -d core ]
            then
              mkdir bin/
              cp -rp core/* bin/
              rm -rf core
            else
              for file in *.xpi
              do
                unzip -o "$file" > /dev/null
              done
              unzip -o "${locale}".xpi > /dev/null
            fi
            update_settings_file='bin/update-settings.ini'
            ;;
        linux|Linux_*)
            if echo "$pkg_file" | grep -q "tar.gz"
            then
                tar xfz ../"$pkg_file" > /dev/null
            elif echo "$pkg_file" | grep -q "tar.bz2"
            then
                tar xfj ../"$pkg_file" > /dev/null
            else
                echo "Unknown package type for file: $pkg_file"
                exit 1
            fi
            update_settings_file=$(echo "$product" | tr '[:upper:]' '[:lower:]')'/update-settings.ini'
            ;;
        *)
            echo "Unknown platform to unpack: $unpack_platform"
            exit 1
    esac

    if [ -n "$unpack_jars" ]; then
        find . \( -name '*.jar' -o -name '*.ja' \) -exec unzip -o {} -d {}.dir \; >/dev/null
    fi

    if [ -n "$update_settings_string" ]; then
       echo "Modifying update-settings.ini"
       sed -e "s/^ACCEPTED_MAR_CHANNEL_IDS.*/ACCEPTED_MAR_CHANNEL_IDS=${update_settings_string}/" < "${update_settings_file}" > "${update_settings_file}.new"
       diff -u "${update_settings_file}" "${update_settings_file}.new"
       echo " "
       rm "${update_settings_file}"
       mv "${update_settings_file}.new" "${update_settings_file}"
    fi

    popd > /dev/null || exit

}
