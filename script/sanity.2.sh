#!/bin/bash

# Exit on error.
set -e
VERBOSE=1

__check_bin() {
    [[ ! -f "${1}" ]] && { echo "[ERROR]: Cannot find '${1}' binary"; exit 1; }

    return 0
}

__log() {
    [[ ${VERBOSE} -eq 1 ]] && echo "$@"

    return 0
}

##########################################################
# main
##########################################################
TSM_NAME=${1-polaris}
PIPE_BIN="src/ltsmpipe"
LTSM_BIN="src/ltsmc"
LTSM_NODE=${TSM_NAME}
LTSM_PASSWORD=${TSM_NAME}
LTSM_SERVERNAME=${2-polaris-kvm-tsm-server}
export DSMI_CONFIG=`pwd`/dsmopt/dsm.sys

PATH_PREFIX=`mktemp -d`

[ ${PWD##*/} == "script" ] && { LTSM_BIN="../${LTSM_BIN}"; } && PIPE_BIN="../${PIPE_BIN}";

__check_bin "${LTSM_BIN}"
__check_bin "${PIPE_BIN}"

echo "Creating sanity data in ${PATH_PREFIX} please wait ..."

MAX_FILE_SIZE_KB=1024

#READ TEST IS WRITTEN VIA LTSMPIPE AND READ VIA LTSMC
RND_FILE_SIZE=$(( (RANDOM % ${MAX_FILE_SIZE_KB}) + 1 ))
__log "Create random file name: ${PATH_PREFIX}/read_test.bin of size: ${RND_FILE_SIZE} KB"
dd if=/dev/urandom of=${PATH_PREFIX}/read_test.bin bs=1K count=${RND_FILE_SIZE} > /dev/null 2>&1

#WRITE TEST IS ARCHIVED VIA LTSMC AND READ VIA LTSMPIPE
RND_FILE_SIZE=$(( (RANDOM % ${MAX_FILE_SIZE_KB}) + 1 ))
__log "Create random file name: ${PATH_PREFIX}/read_test.bin of size: ${RND_FILE_SIZE} KB"
dd if=/dev/urandom of=${PATH_PREFIX}/write_test.bin bs=1K count=${RND_FILE_SIZE} > /dev/null 2>&1

#BOTH TEST IS ARCHIVED VIA LTSMPIPE AND READ VIA LTSMPIPE
RND_FILE_SIZE=$(( (RANDOM % ${MAX_FILE_SIZE_KB}) + 1 ))
__log "Create random file name: ${PATH_PREFIX}/both_test.bin of size: ${RND_FILE_SIZE} KB"
dd if=/dev/urandom of=${PATH_PREFIX}/both_test.bin bs=1K count=${RND_FILE_SIZE} > /dev/null 2>&1

MD5_ORIG="/tmp/md5orig.txt"
MD5_RETR="/tmp/md5retr.txt"

echo "Creating MD5 sum file of original data: ${MD5_ORIG}"
find ${PATH_PREFIX} -exec md5sum -b '{}' \; &> ${MD5_ORIG}
##########################################################
# LTSM actions
##########################################################
# Archive data - always archive one file two times to assure qtable is working with deduplication

echo "Archiving 2 times read_test via ltsmc..."
${LTSM_BIN} --verbose warn --archive -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}/read_test.bin"
${LTSM_BIN} --verbose warn --archive -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}/read_test.bin"
[ $? -eq 0 ] && { echo -e "done\n"; }


echo "Archiving 2 times write_test via ltsmpipe..."
cat ${PATH_PREFIX}/write_test.bin | ${PIPE_BIN} --verbose warn -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}/write_test.bin"
cat ${PATH_PREFIX}/write_test.bin | ${PIPE_BIN} --verbose warn -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}/write_test.bin"
[ $? -eq 0 ] && { echo -e "done\n"; }


echo "Archiving 2 times both_test via ltsmpipe..."
cat ${PATH_PREFIX}/both_test.bin | ${PIPE_BIN} --verbose warn -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "/sanity_test/both_test.bin"
cat ${PATH_PREFIX}/both_test.bin | ${PIPE_BIN} --verbose warn -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "/sanity_test/both_test.bin"
[ $? -eq 0 ] && { echo -e "done\n"; }

# First remove data locally, second retrieve data from TSM storage.
echo "Deleting data locally in ${PATH_PREFIX} and retrieving data from TSM storage"
rm -rf ${PATH_PREFIX}
[ $? -eq 0 ] && { echo -e "done\n"; }

echo "mkdir -p ${PATH_PREFIX}"
mkdir -p ${PATH_PREFIX}

echo "Read read_test via ltsmpipe..."
${PIPE_BIN} --verbose warn -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}/read_test.bin" > ${PATH_PREFIX}/read_test.bin
[ $? -eq 0 ] && { echo -e "done\n"; }

echo "Read write_test via ltsmc..."
${LTSM_BIN} --verbose warn --retrieve -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} --latest "${PATH_PREFIX}/write_test.bin"
[ $? -eq 0 ] && { echo -e "done\n"; }

echo "Read both_test via ltsmpipe..."
${PIPE_BIN} --verbose warn -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "/sanity_test/both_test.bin" > ${PATH_PREFIX}/both_test.bin
[ $? -eq 0 ] && { echo -e "done\n"; }



echo "Creating MD5 sum file of retrieved data: ${MD5_RETR}"
find ${PATH_PREFIX} -exec md5sum -b '{}' \; &> ${MD5_RETR}

${LTSM_BIN} --verbose warn --delete -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}*/*"
${LTSM_BIN} --verbose warn --delete -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "/sanity_test*/*"
[ $? -eq 0 ] && { echo -e "done\n"; }

# Check for equality
ARE_EQUAL=1 # FALSE
if diff -q ${MD5_ORIG} ${MD5_RETR}; then
    ARE_EQUAL=0
fi

# Final result and output
if [ ${ARE_EQUAL} -eq 0 ] ; then
    echo "Sanity successfully finished, archived and retrieved data match."
    rm -rf ${MD5_ORIG} ${MD5_RETR} ${PATH_PREFIX}
else
    echo "Sanity failed, archived and retrieved data does not match."
    rm -rf ${PATH_PREFIX} # Keep the md5sum files to figure out what is going on.
fi

exit ${ARE_EQUAL}