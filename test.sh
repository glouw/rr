make;
for test in tests/*; do
    ./rr $test #1> /dev/null
    echo "$? : $test"
    if [ $? -ne 0 ]; then
        exit 1
    fi
done

exit 0
