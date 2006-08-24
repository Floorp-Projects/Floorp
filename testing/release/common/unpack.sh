#!/bin/bash
unpack_build () {
    unpack_platform="$1"
    dir_name="$2"
    pkg_file="$3"

    mkdir -p $dir_name
    pushd $dir_name > /dev/null
    case $unpack_platform in
        mac|mac-ppc) 
            cd ../
            mkdir -p mnt
            echo "y" | PAGER="/bin/cat"  hdiutil attach \
                -quiet -puppetstrings -noautoopen \ 
                -mountpoint ./mnt "$pkg_file" > /dev/null
            rsync -a ./mnt/* $dir_name/ 
            hdiutil detach mnt > /dev/null
            cd $dir_name
            ;;
        win32) 
            7zr x ../"$pkg_file" > /dev/null
            for file in *.xpi
            do
                unzip -o $file > /dev/null
            done

            ;;
        linux) 
            tar xfvz ../"$pkg_file" > /dev/null
            ;;
    esac

    popd > /dev/null

}
