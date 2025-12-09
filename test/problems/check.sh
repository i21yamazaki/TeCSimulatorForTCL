#!/bin/sh
set -e
tmpfile=$(mktemp)
trap "[ -f $tmpfile ] && rm -f $tmpfile" EXIT
for problem in *
do
    if [ -d $problem ]; then
        for program in $problem/*.t7
        do
            bin=${program%.*}.bin
            nt=${program%.*}.nt
            rm -f $bin $nt
            ( set -x; ../../bin/tasm $program )
            [ -f $bin ]
            [ -f $nt ]
            for casein in $problem/*.in
            do
                caseout=${casein%.*}.out
                if [ -f $caseout ]; then
                    ( set -x; ../../bin/tec $bin $nt < $casein > $tmpfile )
                    cmp $tmpfile $caseout
                fi
            done
        done    
    fi
done
echo "OK"