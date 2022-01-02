#!/bin/sh

# version controll for DLLs
# ToDo: make version parameter or find version from first occurance of 3.x
# make the 3 a variable..., include the header

#OS=`uname -s`
#DSO_SUFFIX=so
#if [ "$OS" = "HP-UX" ]; then
    #DSO_SUFFIX=sl
#fi
#what libnss3.$DSO_SUFFIX | grep NSS
#what libsmime3.$DSO_SUFFIX | grep NSS
#what libssl3.$DSO_SUFFIX | grep NSS
#ident libnss3.$DSO_SUFFIX | grep NSS
#ident libsmime3.$DSO_SUFFIX | grep NSS
#ident libssl3.$DSO_SUFFIX | grep NSS

for w in `find . -name "libnss3.s[ol]" ; find . -name "libsmime3.s[ol]"; find .  -name "libssl3.s[ol]"`
do
        NOWHAT=FALSE
        NOIDENT=FALSE
        echo $w
        what $w | grep NSS || NOWHAT=TRUE
        ident $w | grep NSS || NOIDENT=TRUE
        if [ $NOWHAT = TRUE ]
        then
                echo "ERROR what $w does not contain NSS"
        fi
        if [ $NOIDENT = TRUE ]
        then
                echo "ERROR ident $w does not contain NSS"
        fi
done
#for w in `find . -name "libnss3.s[ol]" ; find . -name "libsmime3.s[ol]"; find .
#-name "libssl3.s[ol]"`
#do
        #NOWHAT=FALSE
        #NOIDENT=FALSE
        #echo $w
        #what $w | grep NSS || NOWHAT=TRUE
        #ident $w | grep NSS || NOIDENT=TRUE
        #if [ $NOWHAT = TRUE -a $NOIDENT = TRUE ]
        #then
                #echo "WARNING what and ident $w does not contain NSS"
                #strings $w  | grep NSS | grep '3.2' || echo "ERROR strings does
#not either..."
        #fi
#done

