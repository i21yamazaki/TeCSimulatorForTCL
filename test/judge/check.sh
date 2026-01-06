#!/bin/sh
set -e
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
                casedst=${casein%.*}.dst
                if [ -f $caseout ]; then
                    ( set -x; ../../bin/tec $bin $nt < $casein > $casedst )
                    cmp $caseout $casedst
                else
                    echo "WARNING: file \"$caseout\" doesn't exist"
                fi
            done
        done    
    fi
done
echo "OK"
