#!/bin/bash
if (( $EUID != 0 )); then
    echo "Please run as root"
    exit
fi

. ./build-allocator.sh

echo "Running tests..."
echo "================"


g++ tests/read_access_test.cpp -o tests/read_access_test.exe -g -O0
./run.sh ./tests/read_access_test.exe tests/read_access_test.in > tests/read_access_test.out 2> tests/read_access_test.err
mv page-tracking.csv tests/read_access_test_page_tracking.csv
mv log.txt tests/read_access_test_log.txt
echo "Test #0 done"

g++ tests/reuse_test.cpp -o tests/reuse_test.exe -g -O3
./run.sh ./tests/reuse_test.exe tests/reuse_test.in > tests/reuse_test.out 2> tests/reuse_test.err
# mv bucket_stats.csv tests/test1_buckets.csv
# mv allocation_site_stats.csv tests/test1_alloc.csv
# mv page_info.csv tests/test1_page_stats.csv
mv page-tracking.csv tests/reuse_test_page_tracking.csv
mv log.txt tests/reuse_test_log.txt
# mv compression.csv tests/reuse_test_compression.csv
# mv object-liveness.csv tests/reuse_test_object_liveness.csv
# mv page-liveness.csv tests/reuse_test_page_liveness.csv
echo "Test #1 done"

g++ tests/pagetest.cpp -o tests/pagetest.exe -g -O3
./run.sh ./tests/pagetest.exe tests/pagetest.in > tests/pagetest.out 2> tests/pagetest.err
# mv bucket_stats.csv tests/pagetest_buckets.csv
# mv allocation_site_stats.csv tests/pagetest_alloc.csv
# mv page_info.csv tests/pagetest_page_stats.csv
mv log.txt tests/pagetest_log.txt
mv page-tracking.csv tests/pagetest_page_tracking.csv
# mv compression.csv tests/pagetest_compression.csv
# mv object-liveness.csv tests/pagetest_object_liveness.csv
# mv page-liveness.csv tests/pagetest_page_liveness.csv
echo "Test #2 done"

g++ tests/test1.cpp -o tests/test1.exe -g -O3
./run.sh ./tests/test1.exe tests/test1.in > tests/test1.out 2> tests/test1.err
# mv bucket_stats.csv tests/test1_buckets.csv
# mv allocation_site_stats.csv tests/test1_alloc.csv
# mv page_info.csv tests/test1_page_stats.csv
mv log.txt tests/test1_log.txt
mv page-tracking.csv tests/test1_page_tracking.csv
# mv compression.csv tests/test1_compression.csv
# mv object-liveness.csv tests/test1_object_liveness.csv
# mv page-liveness.csv tests/test1_page_liveness.csv
echo "Test #3 done"

g++ tests/test2.cpp -pthread -o tests/test2.exe -g -O3
./run.sh ./tests/test2.exe tests/test2.in > tests/test2.out 2> tests/test2.err
# mv bucket_stats.csv tests/test2_buckets.csv
# mv allocation_site_stats.csv tests/test2_alloc.csv
# mv page_info.csv tests/test2_page_stats.csv
mv log.txt tests/test2_log.txt
mv page-tracking.csv tests/test2_page_tracking.csv
# mv compression.csv tests/test2_compression.csv
# mv object-liveness.csv tests/test2_object_liveness.csv
# mv page-liveness.csv tests/test2_page_liveness.csv
echo "Test #4 done"

### gdb commands for debugging executables with hook
# set env LD_PRELOAD=./libbkmalloc.so
# set env BKMALLOC_OPTS="--hooks-file=./hook.so"
# run
