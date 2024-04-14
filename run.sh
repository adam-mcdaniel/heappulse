#!/bin/bash

PROGRAM_TO_RUN="$1"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "$PROGRAM_TO_RUN" ]; then
    echo "Usage: $0 <program to run>"
    exit 1
fi

shift

# gdb setup:
# gdb -ex "set environment LD_PRELOAD=$SCRIPT_DIR/libbkmalloc.so" -ex "set environment BKMALLOC_OPTS=--hooks-file=$SCRIPT_DIR/hook.so" -ex "run $PROGRAM_TO_RUN $@"

# LD_PRELOAD=$SCRIPT_DIR/libbkmalloc.so BKMALLOC_OPTS="--hooks-file=$SCRIPT_DIR/hook.so" "$PROGRAM_TO_RUN" "$@" 0<&-
LD_PRELOAD=$SCRIPT_DIR/libbkmalloc.so BKMALLOC_OPTS="--hooks-file=$SCRIPT_DIR/hook.so --log-hooks" "$PROGRAM_TO_RUN" "$@" 0<&-