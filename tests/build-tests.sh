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


# cd into the build-tests.sh directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
pushd $SCRIPT_DIR > /dev/null

# For every test directory in /tests, compile `test.cpp` and run the executable with the input file `test.in`
for test_dir in *; do
    if [ -d "$test_dir" ]; then
        # Skip directories that don't have a test.cpp file
        if [ ! -f "$test_dir/test.cpp" ]; then
            echo "Skipping test in $test_dir🚨"
            continue
        fi
        echo -ne "Building test in $test_dir🚧"\\r
        g++ $test_dir/test.cpp -o $test_dir/test.exe -g -O0 -pthread || { echo "Failed to compile test in $test_dir❌"; continue; }
        echo "Compiled test in $test_dir✅      "
    fi
done

popd > /dev/null