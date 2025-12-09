#!/bin/sh
set -e

# tasm7でアセンブルした場合と機械語を比べる

if [ -f /usr/local/lib/tasm/Tasm7.jar ]; then
    tasm7="java -Dfile.encoding=UTF-8 -jar /usr/local/lib/tasm/Tasm7.jar"
    for program in *.t7
    do
        if [ -f $program ]; then
            if $tasm7 $program > /dev/null 2>&1; then
                bin=${program%.*}.bin
                original=${program%.*}.original.bin
                mv $bin $original
                tasm $program
                cmp $bin $original
            else
                ! tasm $program 2> /dev/null
            fi
        fi
    done
else
    echo "tasm7 is not installed."
fi
echo "OK"