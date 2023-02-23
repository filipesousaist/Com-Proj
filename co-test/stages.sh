#!/bin/bash
testDir="$(dirname "$0")"		# Test directory
srcDir=$1						# Compiler make-file directory
exe="minor"

# Go to source directory and make compiler
cd $srcDir
srcDir=$(pwd)
{
make clean
make
}  2>&1 | grep "conflict"

# Check conflicts
if [ $? -eq 0 ]; then
	echo "byacc has conflicts, please fix them"
else
	echo "no byacc conflicts, well done"
fi

# Check the compiler itself
if [ ! -f $exe ]; then
    echo "$exe was not found - aborting"
	exit
fi

# Go back to test dir
cd - > /dev/null
cd $testDir

passTotal=0
failTotal=0
passPass=0
failPass=0

passed="\e[32mPASSED\e[0m"
failed="\e[31mFAILED\e[0m"

# These tests should pass
echo " ===== PASS TESTS ===== [should compile]"
for f in $(find test/pass -name "*.min"); do
	"$srcDir/$exe" < $f &> /dev/null
	if [ $? -ne 0 ]; then
		echo -e $failed $f
	else
        ((passPass++))
		echo -e $passed $f
	fi
    ((passTotal++))
done

# Count passed/total number of "fail" tests by stage
STAGES=(LEX GRAM SYMB SEM OTHER)
declare -A stageTotal
declare -A stagePass
for stage in ${STAGES[@]}; do
    stageTotal[${stage}]=0
	stagePass[${stage}]=0
done

# These tests should fail
echo " ===== FAIL TESTS ===== [should not compile]"
for f in $(find test/fail -name "*.min"); do
	stage=$(grep -w ${f##*/} test/fail/README.md | cut -d' ' -f2)
	if [[ -z ${stage} ]]; then
		stage="OTHER"
	fi
	stageTxt="\e[1;35m${stage}\e[0m"
	"$srcDir/$exe" < $f &>/dev/null

	if [ $? -ne 0 ]; then
        ((failPass++))
		((stagePass[$stage]++))
		echo -e $passed $stageTxt $f
	else
		echo -e $failed $stageTxt $f
	fi
    ((failTotal++))
	((stageTotal[$stage]++))
done

echo " ===== STATUS ===== "
echo -e "\e[1;34mPASS\e[0m" tests - passed $passPass/$passTotal
echo -e "\e[1;34mFAIL\e[0m" tests - passed $failPass/$failTotal
echo "----------------------"
echo "Fail tests by stage:"
for stage in ${STAGES[@]}; do
	if [[ ${stageTotal[$stage]} -ne 0 ]]; then
		echo -e "\e[1;35m${stage}\e[0m" - passed ${stagePass[$stage]}/${stageTotal[$stage]}
	fi
done
echo "----------------------"
total=$(($passTotal+$failTotal))
pass=$(($passPass+$failPass))
echo -e "\e[1;34mTOTAL\e[0m" - passed $pass/$total

# Clean source directory
cd $srcDir
# make clean > /dev/null

exit $((($total-$pass)?1:0))
