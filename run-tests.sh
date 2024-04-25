#!/bin/bash
# 
# ██╗  ██╗███████╗ █████╗ ██████╗ ██████╗ ██╗   ██╗██╗     ███████╗███████╗
# ██║  ██║██╔════╝██╔══██╗██╔══██╗██╔══██╗██║   ██║██║     ██╔════╝██╔════╝
# ███████║█████╗  ███████║██████╔╝██████╔╝██║   ██║██║     ███████╗█████╗  
# ██╔══██║██╔══╝  ██╔══██║██╔═══╝ ██╔═══╝ ██║   ██║██║     ╚════██║██╔══╝  
# ██║  ██║███████╗██║  ██║██║     ██║     ╚██████╔╝███████╗███████║███████╗
# ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝      ╚═════╝ ╚══════╝╚══════╝╚══════╝
# 
# ███╗   ███╗███████╗███╗   ███╗ ██████╗ ██████╗ ██╗   ██╗                 
# ████╗ ████║██╔════╝████╗ ████║██╔═══██╗██╔══██╗╚██╗ ██╔╝                 
# ██╔████╔██║█████╗  ██╔████╔██║██║   ██║██████╔╝ ╚████╔╝                  
# ██║╚██╔╝██║██╔══╝  ██║╚██╔╝██║██║   ██║██╔══██╗  ╚██╔╝                   
# ██║ ╚═╝ ██║███████╗██║ ╚═╝ ██║╚██████╔╝██║  ██║   ██║                    
# ╚═╝     ╚═╝╚══════╝╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝                    
# 
# ██████╗ ██████╗  ██████╗ ███████╗██╗██╗     ███████╗██████╗              
# ██╔══██╗██╔══██╗██╔═══██╗██╔════╝██║██║     ██╔════╝██╔══██╗             
# ██████╔╝██████╔╝██║   ██║█████╗  ██║██║     █████╗  ██████╔╝             
# ██╔═══╝ ██╔══██╗██║   ██║██╔══╝  ██║██║     ██╔══╝  ██╔══██╗             
# ██║     ██║  ██║╚██████╔╝██║     ██║███████╗███████╗██║  ██║             
# ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝             
# 

if (( $EUID != 0 )); then
    echo "Please run as root"
    exit
fi

# Cd into the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
pushd $SCRIPT_DIR > /dev/null

# Check if there are arguments. If so, run only the tests in the specified directories
if [ $# -gt 0 ]; then
    echo "Checking if test directories exist🔍..."
    echo "========================================================================="
    for test_dir in "$@"; do
        if [ ! -d "tests/$test_dir" ]; then
            echo "Directory $test_dir does not exist❌"
            echo "Usage: $0 <test directory> <test directory> ..."
            exit 1
        # Else, print the test directory
        else
            echo "Found test $test_dir✅"
        fi
    done
fi

echo ""
echo "Cleaning up🧹"
echo "========================================================================="
./clean.sh tests || { echo "Failed to clean tests❌"; exit 1; }


echo ""
./build-allocator.sh || { echo "Failed to build allocator❌"; exit 1; }

echo ""
echo "Building tests🚧..."
echo "========================================================================="
# Build only the tests that are specified in the arguments
./tests/build-tests.sh $@ || { echo "Failed to build tests❌"; exit 1; }


echo ""
echo "Running tests🧪..."
echo "========================================================================="
# For every test directory in /tests, compile `test.cpp` and run the executable with the input file `test.in`
pushd ./tests > /dev/null
for test_dir in *; do
    if [ -d "$test_dir" ]; then
        # Check if there are arguments. If so, skip the test directories that are not in the arguments
        if [ $# -gt 0 ]; then
            if [[ ! " $@ " =~ "$test_dir" ]]; then
                continue
            fi
        fi
        # Skip directories that don't have a test.cpp file
        if [ ! -f "$test_dir/test.cpp" ]; then
            echo "Skipping test in $test_dir🚨"
            continue
        fi

        echo -ne "Running test in $test_dir🚧"\\r
        rm -Rf $test_dir/results
        mkdir $test_dir/results
        { $SCRIPT_DIR/run.sh $test_dir/test.exe < $test_dir/test.in > $test_dir/results/test.out 2> $test_dir/results/test.err && echo "Test in $test_dir done✅      "; } || { echo "Test in $test_dir failed with non-zero exit code❌"; }

        # Move all the CSV files in the current directory to the test directory
        shopt -s nullglob # This makes *.csv expand to nothing if no files match
        csv_files=(./*.csv)
        if [ ${#csv_files[@]} -gt 0 ]; then
            for file in "${csv_files[@]}"; do
                mv "$file" "$test_dir/results"
            done
        fi
    fi
done
popd > /dev/null

echo ""
echo "All tests done🎉✨"

popd > /dev/null