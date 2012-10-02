#!/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

OS=`uname -s`
ARCH=`uname -p`
SCRIPT_DIR=`pwd`
DATE=`date +%Y-%m-%d`

if [ $# -lt 1 -o $# -gt 3 ]; then
    echo "Usage: $0 [securitytip|securityjes5] <date> <architecture>"
    exit 1
fi

BRANCH="$1"

if [ "${BRANCH}" != "securitytip" -a "${BRANCH}" != "securityjes5" ]; then
    echo "Usage: $0 [securitytip|securityjes5] <date> <architecture>"
    exit 1
fi

if [ $# -ge 2 ]; then
    DATE=$2
fi

if [ $# -ge 3 ]; then
    ARCH=$3
fi

HEADER="Code Coverage - NSS - ${BRANCH} - ${OS}/${ARCH} - ${DATE}"

COV_DIR="/share/builds/mccrel3/security/coverage"
BRANCH_DIR="${COV_DIR}/${BRANCH}"
DATE_DIR="${BRANCH_DIR}/${DATE}-${ARCH}"
CVS_DIR="${DATE_DIR}/cvs_mozilla"
TCOV_DIR="${DATE_DIR}/tcov_mozilla"
OUTPUT="${DATE_DIR}/nss.html"

LIB_PATH="/mozilla/security/nss/lib"
CVS_PATH="${CVS_DIR}${LIB_PATH}"
TCOV_PATH="${TCOV_DIR}${LIB_PATH}"

MIN_GREEN=70
MIN_YELLOW=40

print_header()
{
    echo "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final\">" 
    echo "<HTML><HEAD><TITLE>${HEADER}</TITLE></HEAD><BODY>" 
    echo "<TABLE ALIGN=\"CENTER\" WIDTH=\"100%\">"
    echo "<TR><TH BGCOLOR=\"GREY\"><H2>${HEADER}</H2></TH></TR>"
    echo "</TABLE><BR>"
}

print_footer()
{
    echo "</BODY></HTML>"
}

print_notes()
{
    echo "<TABLE ALIGN=\"CENTER\" WIDTH=\"100%\">"
    echo "<TR ALIGN=\"CENTER\" BGCOLOR=\"LIGHTGREY\"><TD><A HREF=\"http://wikihome.sfbay.sun.com/jes-security/Wiki.jsp?page=Code_Coverage_Test_Execution\">Test Execution Notes</A></TD></TR>"
    echo "</TABLE><BR>"
}

print_legend()
{
    echo "<TABLE ALIGN=\"CENTER\" WIDTH=\"100%\">"
    echo "<TR ALIGN=\"CENTER\" BGCOLOR=\"GREY\"><TH>Legend</TH></TR>"
    echo "<TR ALIGN=\"CENTER\" BGCOLOR=\"LIGHTGREEN\"><TD>${MIN_GREEN}% - 100% of blocks tested</TD></TR>"
    echo "<TR ALIGN=\"CENTER\" BGCOLOR=\"YELLOW\"><TD>${MIN_YELLOW}% - ${MIN_GREEN}% of blocks tested</TD></TR>"
    echo "<TR ALIGN=\"CENTER\" BGCOLOR=\"ORANGE\"><TD>0% - ${MIN_YELLOW}% of blocks tested</TD></TR>"
    echo "<TR ALIGN=\"CENTER\" BGCOLOR=\"RED\"><TD>File not tested (these files are not included into statistics)</TD></TR>"
    echo "</TABLE>"
}

set_color()
{
    if [ ${PERCENT_INT} -le ${MIN_YELLOW} ]; then
        bgcolor="ORANGE"
    elif [ ${PERCENT_INT} -le ${MIN_GREEN} ]; then
        bgcolor="YELLOW"
    else
        bgcolor="LIGHTGREEN"
    fi    
}

create_table()
{
    echo "<TABLE ALIGN=\"CENTER\" WIDTH=\"100%\">"
    echo "<TR><TH BGCOLOR=\"GREY\" COLSPAN=\"2\">${DIR}</TH></TR>"
    echo "<TR BGCOLOR=\"DARKGREY\"><TH WIDTH=\"50%\">File</TH>"
    echo "<TH>Tested blocks (Tested blocks/Total blocks/Total lines)</TR>"
}

close_table()
{
    if [ "${LASTDIR}" != "" ]; then
        if [ ${DFILES} -gt 0 ]; then
            if [ ${DBLOCKS_TOTAL} -eq 0 ]; then
                PERCENT_INT=0
            else
                PERCENT_INT=`expr ${DBLOCKS_EXEC} \* 100 \/ ${DBLOCKS_TOTAL}`
            fi
            set_color

            echo "<TR><TH BGCOLOR=\"${bgcolor}\" COLSPAN=\"2\">Total: ${PERCENT_INT}% (${DBLOCKS_EXEC}/${DBLOCKS_TOTAL})</TH></TR>"
        else
            echo "<TR><TH BGCOLOR=\"RED\" COLSPAN=\"2\">Total: Not tested</TH></TR>"
        fi
        echo "</TABLE><BR>"
    fi
}

print_line()
{
    LINES_TOTAL=`wc -l "${file}" | /usr/bin/awk '{print $1}'`

    if [ -r "${TCOV_PATH}/${DIR}/${FILE}" ]; then
        BLOCKS_EXEC=`cat "${TCOV_PATH}/${DIR}/${FILE}" | grep "Basic blocks executed" | /usr/bin/awk '{print $1}'`
        BLOCKS_TOTAL=`cat "${TCOV_PATH}/${DIR}/${FILE}" | grep "Basic blocks in this file" | /usr/bin/awk '{print $1}'`

        DBLOCKS_EXEC=`expr ${DBLOCKS_EXEC} + ${BLOCKS_EXEC}`
        DBLOCKS_TOTAL=`expr ${DBLOCKS_TOTAL} + ${BLOCKS_TOTAL}`
        TBLOCKS_EXEC=`expr ${TBLOCKS_EXEC} + ${BLOCKS_EXEC}`
        TBLOCKS_TOTAL=`expr ${TBLOCKS_TOTAL} + ${BLOCKS_TOTAL}`

        TFILES=`expr ${TFILES} + 1`
        DFILES=`expr ${DFILES} + 1`

        PERCENT_EXEC=`cat "${TCOV_PATH}/${DIR}/${FILE}" | grep "Percent of the file executed" | /usr/bin/awk '{print $1}'`
        PERCENT_INT=`echo ${PERCENT_EXEC} | cut -d. -f1`
        set_color

        echo "<TR><TD BGCOLOR=\"LIGHTGREY\"><A HREF=\"${TCOV_PATH}/${DIR}/${FILE}\">${FILE}</A></TD>"
        echo "<TD BGCOLOR=\"${bgcolor}\">${PERCENT_EXEC}% (${BLOCKS_EXEC}/${BLOCKS_TOTAL}/${LINES_TOTAL})</TD></TR>"
    else
        echo "<TR><TD BGCOLOR=\"LIGHTGREY\"><A HREF=\"${file}\">${FILE}</A></TD>"
        echo "<TD BGCOLOR=\"RED\">Not tested (0/?/${LINES_TOTAL})</TD></TR>"
    fi
}

print_total()
{
    echo "<TABLE ALIGN=\"CENTER\" WIDTH=\"100%\">"
    if [ ${TFILES} -gt 0 ]; then
        if [ ${TBLOCKS_TOTAL} -eq 0 ]; then
            PERCENT_INT=0
        else
            PERCENT_INT=`expr ${TBLOCKS_EXEC} \* 100 \/ ${TBLOCKS_TOTAL}`
        fi
        set_color

        echo "<TR><TH BGCOLOR=\"${bgcolor}\"><H2>Total: ${PERCENT_INT}% (${TBLOCKS_EXEC}/${TBLOCKS_TOTAL})</H2></TH></TR>"
    else
        echo "<TR><TH BGCOLOR=\"RED\"><H2>Total: Not tested</H2></TH></TR>"
    fi
    echo "</TABLE><BR>"
}

process_cmd()
{
    LASTDIR=""
    TBLOCKS_EXEC=0
    TBLOCKS_TOTAL=0
    TFILES=0

    for dir in `find "${CVS_PATH}" -type d | sort`
    do
        DIR=`echo "${dir}" | sed "s:^${CVS_PATH}/::"`
        for file in `ls -1 ${dir}/*.c 2> /dev/null`
        do 
            if [ "${DIR}" != "${LASTDIR}" ]; then
                close_table
                create_table 

                LASTDIR="${DIR}";
                DBLOCKS_EXEC=0
                DBLOCKS_TOTAL=0
                DFILES=0
            fi

            FILE=`echo "${file}" | sed "s:^.*/\(.*.c\):\1:"`
            print_line
        done
    done

    close_table
    print_total
}

report()
{
    print_header > "${OUTPUT}"
    print_notes >> "${OUTPUT}"
    process_cmd >> "${OUTPUT}"
    print_legend >> "${OUTPUT}"
    print_footer >> "${OUTPUT}"
}

report

exit 0
