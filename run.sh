#!/bin/bash

PROGRAM_TO_RUN="$1"
INPUT_FILE="$2"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "$PROGRAM_TO_RUN" ]; then
    echo "Usage: $0 <program to run> <input file>"
    exit 1
fi

if [ -z "$INPUT_FILE" ]; then
    LD_PRELOAD=$SCRIPT_DIR/libbkmalloc.so BKMALLOC_OPTS="--hooks-file=$SCRIPT_DIR/hook.so --log-hooks" "$PROGRAM_TO_RUN"
else
    LD_PRELOAD=$SCRIPT_DIR/libbkmalloc.so BKMALLOC_OPTS="--hooks-file=$SCRIPT_DIR/hook.so --log-hooks" "$PROGRAM_TO_RUN" < "$INPUT_FILE"
fi

