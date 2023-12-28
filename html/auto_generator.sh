#!/bin/bash

DEST_FILE=HtmlResource.h
TMP_FILE=tmp.html
SOURCE_FILES=("html_config_success.html" "html_config_wifi.html")

remove_unsupport_character() {
    tr -d '\r\n' < $1 > ${TMP_FILE}
    sed -i "s/\n//g" ${TMP_FILE}
    sed -i "s/\"/'/g" ${TMP_FILE}
    sed -i "s/\t//g" ${TMP_FILE}
    sed -i 's/  */ /g' ${TMP_FILE}

    cat ${TMP_FILE}
    rm -rf ${TMP_FILE}
}

echo "#pragma once" > ${DEST_FILE}
echo "" >> ${DEST_FILE}
echo "#include <Arduino.h>" >> ${DEST_FILE}
echo "" >> ${DEST_FILE}


for file in "${SOURCE_FILES[@]}"
do
    filename="${file%.*}"
    filename="${filename^^}"
    
    content=`remove_unsupport_character $file`
    echo "const char $filename[] PROGMEM = \"${content}\";" >> ${DEST_FILE}
done