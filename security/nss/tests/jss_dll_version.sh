#!/bin/sh

# version controll for DLLs
# ToDo: make version parameter or find version from first occurance of 3.x
# make the 3 a variable..., include the header

for w in `find . -name "libjss3.s[ol]"`
do
        NOWHAT=FALSE
        NOIDENT=FALSE
        echo $w
        what $w | grep JSS || NOWHAT=TRUE
        ident $w | grep JSS || NOIDENT=TRUE
        if [ $NOWHAT = TRUE ]
        then
                echo "ERROR what $w does not contain JSS"
        fi
        if [ $NOIDENT = TRUE ]
        then
                echo "ERROR ident $w does not contain JSS"
        fi
done
