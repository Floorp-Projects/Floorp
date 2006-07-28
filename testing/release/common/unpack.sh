#!/bin/bash
unpack_build () {
    platform="$1"
    dir_name="$2"
    pkg_file="$3"

    mkdir -p $dir_name
    pushd $dir_name > /dev/null
    case $platform in
        mac|mac-ppc) 
            cd ../
            mkdir -p mnt
            echo "y" | PAGER="cat > /dev/null"  hdiutil attach \
                -quiet -mountpoint ./mnt "$pkg_file" 
            sleep 5
            rsync -a ./mnt/* $dir_name/ 
            hdiutil detach mnt
            cd $dir_name
            ;;
        win32) 
            /usr/local/bin/7za x ../"$pkg_file" > /dev/null
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
