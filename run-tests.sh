. ./build-allocator.sh

echo "Running tests..."
echo "================"

# g++ tests/pagetest.cpp -o tests/pagetest.exe -g -O0
# ./run.sh valgrind ./tests/pagetest.exe tests/pagetest.in > tests/pagetest.out 2> tests/pagetest.err
# mv bucket_stats.csv tests/pagetest_buckets.csv
# mv allocation_site_stats.csv tests/pagetest_alloc.csv
# mv page_info.csv tests/pagetest_page_stats.csv
# echo "Test #1 done"

g++ tests/test1.cpp -o tests/test1.exe -g -O0
./run.sh ./tests/test1.exe tests/test1.in > tests/test1.out 2> tests/test1.err
mv bucket_stats.csv tests/test1_buckets.csv
mv allocation_site_stats.csv tests/test1_alloc.csv
mv page_info.csv tests/test1_page_stats.csv
echo "Test #2 done"

g++ tests/test2.cpp -pthread -o tests/test2.exe -g -O0
./run.sh ./tests/test2.exe tests/test2.in > tests/test2.out 2> tests/test2.err
mv bucket_stats.csv tests/test2_buckets.csv
mv allocation_site_stats.csv tests/test2_alloc.csv
mv page_info.csv tests/test2_page_stats.csv
echo "Test #3 done"

### gdb commands for debugging executables with hook
# set env LD_PRELOAD=./libbkmalloc.so
# set env BKMALLOC_OPTS="--hooks-file=./hook.so"
# run
