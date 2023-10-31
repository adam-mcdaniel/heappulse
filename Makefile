GDB=gdb
CC=gcc
CXX=g++
OBJCOPY=objcopy

### DEBUG
FP=-fno-omit-frame-pointer # enable for profiling/debugging
BK_DEBUG=-DBK_DEBUG
DEBUG=-g $(FP) -DBK_DEBUG

### OPT
# LTO="-flto"
# MARCHTUNE="-march=native -mtune=native"
# OPT_PASSES=""
# LEVEL="-O3"
LEVEL=-O0
OPT=$(LEVEL) $(OPT_PASSES) $(MARCHTUNE) $(LTO)

### FEATURES
MMAP_OVERRIDE=-DBK_MMAP_OVERRIDE
RETURN_ADDR=-DBK_RETURN_ADDR # not implemented
# ASSERT="-DBK_DO_ASSERTIONS"
FEATURES=$(MMAP_OVERRIDE) $(RETURN_ADDR) $(ASSERT)

### FLAGS
WARN_FLAGS=-Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-function -Wno-unused-value
MAX_ERRS=-fmax-errors=3
TLS_MODEL=-ftls-model=initial-exec
BKMALLOC_FLAGS=$(TLS_MODEL) $(DEBUG) $(OPT) $(WARN_FLAGS) $(MAX_ERRS) $(FEATURES) -fno-rtti -fPIC -ldl 
CXX_FLAGS= -I./include -I./src/include -lz -g $(OPT) $(FEATURES)
HOOK_FLAGS=-fPIC -shared $(FEATURES)


# CFLAGS=-g -O3 -Wall -Wextra -pedantic -Werror -std=c2x -march=rv64gc -mabi=lp64d -ffreestanding -nostdlib -nostartfiles -Iinclude -mcmodel=medany
ASM_DIR=asm
SOURCE_DIR=src
OBJ_DIR=objs
DEP_DIR=deps
SOURCES=$(wildcard $(SOURCE_DIR)/*.cpp)
OBJECTS=$(patsubst $(SOURCE_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS=$(patsubst $(SOURCE_DIR)/%.cpp,$(DEP_DIR)/%.d,$(SOURCES))

all: hook

include $(wildcard $(DEP_DIR)/*.d)

bkmalloc: clean
	$(CXX) -g -I./include -shared -o $(OBJ_DIR)/libbkmalloc.so -x c++ include/bkmalloc.h  -DBKMALLOC_IMPL -DBK_RETURN_ADDR $(BKMALLOC_FLAGS) $(CXX_FLAGS)

hook: bkmalloc $(OBJECTS)
	$(CXX) $(HOOK_FLAGS) $(CXX_FLAGS) -L$(OBJ_DIR) src/hook.cpp -o $(OBJ_DIR)/hook.so

$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.cpp Makefile
	$(CXX) $(HOOK_FLAGS) $(CXX_FLAGS) -o $@ -c $<
	
$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.c Makefile
	$(CXX) $(HOOK_FLAGS) $(C_FLAGS) -o $@ -c $<
	
.PHONY: clean

clean:
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/*.so $(DEP_DIR)/*.d
