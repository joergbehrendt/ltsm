#!/bin/bash

# Exit on error.
set -e
VERBOSE=0

__check_bin() {
    [[ ! -f "${1}" ]] && { echo "[ERROR]: Cannot find '${1}' binary"; exit 1; }

    return 0
}

__log() {
    [[ ${VERBOSE} -eq 1 ]] && echo "$@"

    return 0
}

__rnd_string()
{
    L=${1}
    cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w ${1:-${L}} | head -n 1
}

__rnd_dirs()
{
    MAX_DIR_DEPTH=${1}
    DIR_LENGTH=${2}

    D=$(( (RANDOM % ${MAX_DIR_DEPTH}) + 1 ))
    P="${PATH_PREFIX}/"

    for d in $(seq 1 ${D}); do
	P+=$(__rnd_string ${DIR_LENGTH})
	P+="/"
    done

    echo ${P}
}

__rnd_files()
{
    # Create randomly at most ${1} files within the last nested directory (can also be 0).
    MAX_NUM_FILES=${1}

    # Create randomly at most ${2} nested directories each have random file name of length ${3}.
    RND_DIR=$(__rnd_dirs ${2} ${3})

    mkdir -p ${RND_DIR}
    __log "Create random nested directories: ${RND_DIR}"
    __log "Create ${F} file(s) with the last nested directory"

    F=$(( (RANDOM % ${MAX_NUM_FILES}) ))
    MAX_FILE_SIZE_KB=1024
    for f in $(seq 1 ${F}); do
	RND_FN=$(__rnd_string 4) # Create a random file name of length 4
	RND_FILE_SIZE=$(( (RANDOM % ${MAX_FILE_SIZE_KB}) + 1 ))

	__log "Create random file name: ${RND_FN} of size: ${RND_FILE_SIZE} KB in last directory"
	dd if=/dev/urandom of=${RND_DIR}/${RND_FN} bs=1K count=${RND_FILE_SIZE} > /dev/null 2>&1
    done
}

##########################################################
# main
##########################################################
TSM_NAME=${1-polaris}
LTSM_BIN="src/ltsmc"
LTSM_NODE=${TSM_NAME}
LTSM_PASSWORD=${TSM_NAME}
LTSM_SERVERNAME=${2-polaris-kvm-tsm-server}
LTSM_VERBOSE=${3-warn}

PATH_PREFIX=`mktemp -d`
MAX_NUM_FILES=35
MAX_NESTED_DIRS=16
MAX_DIR_LEN=6

[ ${PWD##*/} == "script" ] && { LTSM_BIN="../${LTSM_BIN}"; }
__check_bin "${LTSM_BIN}"

echo "Creating sanity data in ${PATH_PREFIX} please wait ..."

##########################################################
# Create directories and files
##########################################################
for r in $(seq 1 5); do
    __rnd_files ${MAX_NUM_FILES} ${MAX_NESTED_DIRS} ${MAX_DIR_LEN}
done


##########################################################
# Create directories with empty files
##########################################################
MAX_NESTED_DIRS=5
MAX_DIR_LEN=7
MAX_DIRS=4
for r in $(seq 1 ${MAX_DIRS}); do
    DIR=$(__rnd_dirs ${MAX_NESTED_DIRS} ${MAX_DIR_LEN})
    mkdir -p ${DIR}

    MAX_EMPTY_FILES=$(( (RANDOM % ${MAX_NUM_FILES}) + 1 ))
    __log "Create ${MAX_EMPTY_FILES} files"
    for i in $(seq 1 ${MAX_EMPTY_FILES}); do
	FILE=$(__rnd_string 4)
	touch ${DIR}/${FILE}
	__log "Create empty file: ${DIR}/${FILE}"
    done
done

##########################################################
# Create empty directories
##########################################################
for r in $(seq 1 ${MAX_DIRS}); do
    DIR=$(__rnd_dirs ${MAX_NESTED_DIRS} ${MAX_DIR_LEN})
    mkdir -p ${DIR}
done

echo "Total number of directories      : `find ${PATH_PREFIX} -type d | wc -l`"
echo "Total number of empty directories: `find ${PATH_PREFIX} -type d -empty | wc -l`"
echo "Total number of files            : `find ${PATH_PREFIX} -type f | wc -l`"
echo "Total number of empty files      : `find ${PATH_PREFIX} -type f -empty | wc -l`"

MD5_ORIG="/tmp/md5orig.txt"
MD5_RETR="/tmp/md5retr.txt"

echo "Creating MD5 sum file of original data: ${MD5_ORIG}"
find ${PATH_PREFIX} -exec md5sum -b '{}' \; |& sort > ${MD5_ORIG}

##########################################################
# LTSM actions
##########################################################
# Archive data
echo "Archiving data please wait ..."
${LTSM_BIN} --verbose ${LTSM_VERBOSE} --archive -r -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}"
[ $? -eq 0 ] && { echo -e "done\n"; }

# First remove data locally, second retrieve data from TSM storage.
echo "Deleting data locally in ${PATH_PREFIX} and retrieving data from TSM storage"
rm -rf ${PATH_PREFIX}
${LTSM_BIN} --verbose ${LTSM_VERBOSE} --retrieve -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}*/*"
[ $? -eq 0 ] && { echo -e "done\n"; }

echo "Creating MD5 sum file of retrieved data: ${MD5_RETR}"
find ${PATH_PREFIX} -exec md5sum -b '{}' \; |& sort > ${MD5_RETR}

# Finally remove data locally and also from TSM storage.
rm -rf ${PATH_PREFIX}
${LTSM_BIN} --verbose ${LTSM_VERBOSE} --delete -f '/' -n ${LTSM_NODE} -p ${LTSM_PASSWORD} -s ${LTSM_SERVERNAME} "${PATH_PREFIX}*/*"
[ $? -eq 0 ] && { echo -e "done\n"; }

# Check for equality
ARE_EQUAL=1 # FALSE
if diff -q ${MD5_ORIG} ${MD5_RETR}; then
    ARE_EQUAL=0
fi

# Final result and output
if [ ${ARE_EQUAL} -eq 0 ] ; then
    echo -e "\n\033[0;32mSanity successfully finished. Archived and retrieved data match.\033[0m"
    rm -rf ${MD5_ORIG} ${MD5_RETR}
else
    echo -e "\n\033[0;31mSanity failed. Archived and retrieved data does not match.\033[0m"
     # Keep the md5sum files to figure out what is going on.
fi

exit ${ARE_EQUAL}
