g++ tests/test1.cpp -o test1.exe -g
./run.sh ./test1.exe tests/test1.in > tests/test1.out 2> tests/test1.err

g++ tests/test2.cpp -pthread -o test2.exe -g
./run.sh ./test2.exe tests/test2.in > tests/test2.out 2> tests/test2.err
