echo "Fail tests not documented in README.md:"
for f in $(find test/fail -name "*.min"); do
    line=$(grep ${f##*/} test/fail/README.md)
    if [[ -z $line ]]; then
        echo ${f##*/}
    fi
done
