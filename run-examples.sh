g++ tests/test1.cpp -o test -g
./run.sh ./test1 tests/test1.in

g++ tests/test2.cpp -pthread -o test2 -g
./run.sh ./test2 tests/test2.in
