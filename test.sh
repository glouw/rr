make;
for test in tests/*; do
    ./rr $test
    echo "$? : $test"
    if [ $? -ne 0 ]; then
        exit 1
    fi
done

exit 0
