make install
for dir in tests leetcode; do
    cd $dir
    for test in *.rr; do
        roman2 $test 1> /dev/null
        echo "$? : $test"
        if [ $? -ne 0 ]; then
            exit 1
        fi
    done
    cd -
done

exit 0
