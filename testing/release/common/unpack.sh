#!/bin/bash
unpack_build () {
    unpack_platform="$1"
    dir_name="$2"
    pkg_file="$3"

    mkdir -p $dir_name
    pushd $dir_name > /dev/null
    case $unpack_platform in
        mac|mac-ppc|Darwin_ppc-gcc|Darwin_Universal-gcc3)
            cd ../
            mkdir -p mnt
            echo "mounting $pkg_file"
            echo "y" | PAGER="/bin/cat"  hdiutil attach -quiet -puppetstrings -noautoopen -mountpoint ./mnt "$pkg_file" > /dev/null
            rsync -a ./mnt/* $dir_name/ 
            hdiutil detach mnt > /dev/null
            cd $dir_name
            ;;
        win32|WINNT_x86-msvc)
            7z x ../"$pkg_file" > /dev/null
            if [ -d localized ]
            then
              mkdir bin/
              cp -rp localized/* nonlocalized/* optional/* bin/
            else
              for file in *.xpi
              do
                unzip -o $file > /dev/null
              done
            fi
            ;;
        linux-i686|linux|Linux_x86-gcc|Linux_x86-gcc3)
            tar xfz ../"$pkg_file" > /dev/null
            ;;
    esac

    popd > /dev/null

}
