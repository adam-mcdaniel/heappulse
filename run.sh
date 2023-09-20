#!/bin/bash

PROGRAM_TO_RUN="$1"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "$PROGRAM_TO_RUN" ]; then
    echo "Usage: $0 <program to run>"
    exit 1
fi

shift

LD_PRELOAD=$SCRIPT_DIR/libbkmalloc.so BKMALLOC_OPTS="--hooks-file=$SCRIPT_DIR/hook.so" "$PROGRAM_TO_RUN" "$@" 0<&-