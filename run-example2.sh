cp examples/zswap_compression.cpp .
g++ -fPIC -shared zswap_compression.cpp -L./libbkmalloc.so -o hook.so -lz -g
rm zswap_compression.cpp
g++ test1.cpp -pthread -o test1 -g
LD_PRELOAD=./libbkmalloc.so BKMALLOC_OPTS="--hooks-file=./hook.so --log-hooks" ./test1