#!/bin/bash
echo "Building allocator..."

### DEBUG
    FP="-fno-omit-frame-pointer" # enable for profiling/debugging
    BK_DEBUG="-DBK_DEBUG"
    DEBUG="-g ${FP} -DBK_DEBUG"

### OPT
    # LTO="-flto"
    # MARCHTUNE="-march=native -mtune=native"
    # OPT_PASSES=""
    LEVEL="-O3"
    # LEVEL="-O0"

    OPT="${LEVEL} ${OPT_PASSES} ${MARCHTUNE} ${LTO}"

### FEATURES
    MMAP_OVERRIDE="-DBK_MMAP_OVERRIDE"
    RETURN_ADDR="-DBK_RETURN_ADDR" # not implemented
    # ASSERT="-DBK_DO_ASSERTIONS"

    FEATURES="${MMAP_OVERRIDE} ${RETURN_ADDR} ${ASSERT}"

WARN_FLAGS="-Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-function -Wno-unused-value"
MAX_ERRS="-fmax-errors=3"
TLS_MODEL="-ftls-model=initial-exec"
C_FLAGS="-fPIC ${TLS_MODEL} ${DEBUG} ${OPT} ${WARN_FLAGS} ${MAX_ERRS} ${FEATURES} -ldl"
CPP_FLAGS="-fno-rtti ${C_FLAGS} -lz"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPILE="g++ -g -I./include -shared -o ${SCRIPT_DIR}/libbkmalloc.so -x c++ include/bkmalloc.h  -DBKMALLOC_IMPL -DBK_RETURN_ADDR ${CPP_FLAGS}"
${COMPILE} || exit $?

g++ -fPIC -shared src/hook.cpp ${FEATURES} -I./include -I./src/include -L$SCRIPT_DIR/libbkmalloc.so -o $SCRIPT_DIR/hook.so -lz -g

# g++ -fPIC -shared src/hook.cpp ${FEATURES} -I./include -I./src/include -L$SCRIPT_DIR/libbkmalloc.so -o $SCRIPT_DIR/hook.so -lz -g
# g++ -fPIC -L*.o ${FEATURES} -I./include -I./src/include -c -o hook.o -lz -g
# g++ -shared -Wl,--whole-archive hook.o -Wl,--no-whole-archive -o hook.so -L$SCRIPT_DIR/libbkmalloc.so


# echo "Done"
# make
# cp objs/libbkmalloc.so .
# cp objs/hook.so .
echo "Done"