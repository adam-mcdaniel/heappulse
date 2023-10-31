#!/usr/bin/env bash
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
CPP_FLAGS="-fno-rtti ${C_FLAGS}"

COMPILE="g++ -g -I./include -shared -o libbkmalloc.so -x c++ include/bkmalloc.h  -DBKMALLOC_IMPL -DBK_RETURN_ADDR ${CPP_FLAGS}"
${COMPILE} || exit $?

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
g++ -fPIC -shared src/zswap_compression.cpp ${FEATURES} -I./include -L$SCRIPT_DIR/libbkmalloc.so -o $SCRIPT_DIR/hook.so -lz -g


echo "Done"