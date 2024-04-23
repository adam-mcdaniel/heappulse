#!/bin/bash
if (( $EUID != 0 )); then
    echo "Please run as root"
    exit
fi

. ./build-allocator.sh

echo "Running tests..."
echo "================"


# For every test directory in /tests, compile `test.cpp` and run the executable with the input file `test.in`
for test_dir in tests/*; do
    if [ -d "$test_dir" ]; then
        echo "Running test in $test_dir"
        g++ $test_dir/test.cpp -o $test_dir/test.exe -g -O0 -pthread
        rm -Rf $test_dir/results
        mkdir $test_dir/results
        ./run.sh $test_dir/test.exe < $test_dir/test.in > $test_dir/results/test.out 2> $test_dir/results/test.err
        echo "Test in $test_dir done"
        # Move all the CSV files in the current directory to the test directory
        shopt -s nullglob # This makes *.csv expand to nothing if no files match
        csv_files=(./*.csv)
        if [ ${#csv_files[@]} -gt 0 ]; then
            for file in "${csv_files[@]}"; do
                mv "$file" "$test_dir/results"
            done
        fi

        echo "Test in $test_dir done"
    fi
done