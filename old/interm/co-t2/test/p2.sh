#!/bin/bash
root=tests_p2

# Test part 1
output=$(./p1.sh)
if [[ $? -ne 0 ]]; then
    echo "part 1 tests not passing:"
    echo "$output" | grep FAILED
    exit 1
else
    echo "part 1 tests passing"
fi

count=$(find "$root" | grep test.min | wc -l)
echo "$count tests were detected"

# Compile
cd ../src
srcDir=$(pwd)
{ make clean; make; } > /dev/null 
if [[ $? -ne 0 ]]; then
    echo "make failed!"
    exit 1
fi
cd - > /dev/null

function testDir {
    echo -e "\e[33m$(basename $1)\e[0m"
    local save=$(pwd)
    cd $1
    if [[ -f "test.min" ]]; then
        
        # Generate program
        this=$(pwd)
        $srcDir/minor < "test.min" &> /dev/null
        cp out.asm $srcDir
        cd $srcDir
        make out &> /dev/null
        mv out $this/a.out
        cd - &> /dev/null
        
        # Look into test folders
        for d in $(ls -d t*/); do
            cd $d

            # Run test
            ../a.out < "in" > "myout"
            echo $? > "myret"

            # Check output
            diff "myout" "out" > "diff"
            equal=$?
            
            # Assume ret code of 0
            if [[ ! -f "ret" ]]; then
                echo 0 > "ret"
            fi

            diff "myret" "ret" -q &>/dev/null
            equalRet=$?
            
            if [[ $equal -eq 0 ]] && [[ $equalRet -eq 0 ]]; then
                echo -e "\e[36m$d\e[0m - \e[32mPASSED\e[0m"
                ((pass++))
            else
                echo -e "\e[36m$d\e[0m - \e[31mFAILED\e[0m"
                if [[ $equal -ne 0 ]]; then
                    echo -e "out files differ"
                fi
                if [[ $equalRet -ne 0 ]]; then
                    echo -e "ret code differs: expected $(cat "ret") but got $(cat "myret")"
                fi
            fi
            ((total++))
            cd - > /dev/null
        done

    fi
    cd $save
}

pass=0
total=0
cd $root

for l in $(ls -d */ 2>/dev/null); do
    testDir "$l"
done

echo "passed $pass/$total"
