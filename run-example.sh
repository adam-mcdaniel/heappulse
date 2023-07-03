cp examples/zswap_compression.cpp .
g++ -fPIC -shared zswap_compression.cpp -L./libbkmalloc.so -o hook.so -lz -g
rm zswap_compression.cpp
g++ test.cpp -o test -g
LD_PRELOAD=./libbkmalloc.so BKMALLOC_OPTS="--hooks-file=./hook.so --log-hooks" ./test