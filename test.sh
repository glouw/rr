make;
cd tests
for test in *; do
    roman2 $test 1> /dev/null
    echo "$? : $test"
    if [ $? -ne 0 ]; then
        exit 1
    fi
done
cd -

exit 0
