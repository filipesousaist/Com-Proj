#!/bin/bash

root=test-2
count=$(find "$root" | grep test.min | wc -l)

echo "$count tests were detected"
echo

level=0
pass=0
total=0

function secho {
    for d in $(seq 1 $level); do
        printf "  "
    done
    printf "$1\n"
}

function testDir {
    secho "\e[33m$(basename $1)\e[0m"
    ((level++))
    local save=$(pwd)
    cd $1
    if [[ -f "test.min" ]]; then
        for f in $(ls | grep "\.in"); do
            local base=$(basename $f .in)
            outfile=$base.myout
            touch $outfile
            passed=0
            if [[ -z $passed ]]; then
                secho "\e[36m$f\e[0m - \e[32mPASSED\e[0m"
                ((pass++))
            else
                secho "\e[36m$f\e[0m - \e[31mFAILED\e[0m (see $base.myout)"
            fi
            ((total++))
        done
    fi
    for f in $(ls -d */ 2>/dev/null); do
        testDir $f
    done
    cd $save
    ((level--))
}

results=""
cd $root
for l in $(ls -d */ 2>/dev/null); do
   pass=0
   total=0
   testDir "$l"
   l=$(basename $l)
   results=$results"$l\tpassed $passed/$total\n"
done

echo
echo -e "\e[1;35mresults\e[0m\n"$results | column -ts $'\t'
