# 
# ██╗  ██╗███████╗ █████╗ ██████╗ ██████╗ ██╗   ██╗██╗     ███████╗███████╗
# ██║  ██║██╔════╝██╔══██╗██╔══██╗██╔══██╗██║   ██║██║     ██╔════╝██╔════╝
# ███████║█████╗  ███████║██████╔╝██████╔╝██║   ██║██║     ███████╗█████╗  
# ██╔══██║██╔══╝  ██╔══██║██╔═══╝ ██╔═══╝ ██║   ██║██║     ╚════██║██╔══╝  
# ██║  ██║███████╗██║  ██║██║     ██║     ╚██████╔╝███████╗███████║███████╗
# ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝      ╚═════╝ ╚══════╝╚══════╝╚══════╝
# 
# ███╗   ███╗███████╗███╗   ███╗ ██████╗ ██████╗ ██╗   ██╗                 
# ████╗ ████║██╔════╝████╗ ████║██╔═══██╗██╔══██╗╚██╗ ██╔╝                 
# ██╔████╔██║█████╗  ██╔████╔██║██║   ██║██████╔╝ ╚████╔╝                  
# ██║╚██╔╝██║██╔══╝  ██║╚██╔╝██║██║   ██║██╔══██╗  ╚██╔╝                   
# ██║ ╚═╝ ██║███████╗██║ ╚═╝ ██║╚██████╔╝██║  ██║   ██║                    
# ╚═╝     ╚═╝╚══════╝╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝                    
# 
# ██████╗ ██████╗  ██████╗ ███████╗██╗██╗     ███████╗██████╗              
# ██╔══██╗██╔══██╗██╔═══██╗██╔════╝██║██║     ██╔════╝██╔══██╗             
# ██████╔╝██████╔╝██║   ██║█████╗  ██║██║     █████╗  ██████╔╝             
# ██╔═══╝ ██╔══██╗██║   ██║██╔══╝  ██║██║     ██╔══╝  ██╔══██╗             
# ██║     ██║  ██║╚██████╔╝██║     ██║███████╗███████╗██║  ██║             
# ╚═╝     ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝╚══════╝╚══════╝╚═╝  ╚═╝             
# 

# Create a CMake file for the allocation tracker.
# The main file for the project is `src/hook.cpp`
# The stack containers library is in `src/include`,
# the interval tests are in `src/intervals`, and
# the tests are in `src/tests`.

# Set the minimum version of CMake required to build the project.
cmake_minimum_required(VERSION 3.10)

# Set the project name and version.
project(HeapPulse VERSION 1.0)

# Set the C++ standard to C++17.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "g++")

# Set the compiler to use -fPIC, -ftls-model=initial-exec,
# -fno-omit-frame-pointer, -Wno-missing-field-initializers,
# -Wno-unused-parameter, -Wno-unused-value, -Wno-unused-function,
# -fno-trapping-math, along with some other -D flags.

# First, define the -D flags.
add_definitions(-DBK_MMAP_OVERRIDE -DBK_RETURN_ADDR -DBKMALLOC_IMPL)

# If we're doing a debug build, add the debug flag.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    add_definitions(-DBK_DEBUG)
endif()

set(WARNING_FLAGS "-fmax-errors=3 -Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-unused-function -Wno-missing-field-initializers -Wno-trapping-math")
set(OPTIMIZATION_FLAGS "-O3 -g")
set(SHARED_FLAGS "-ftls-model=initial-exec -fno-omit-frame-pointer")

# Set the flags.
set(CMAKE_CXX_FLAGS "${SHARED_FLAGS} ${WARNING_FLAGS} ${OPTIMIZATION_FLAGS} ${CMAKE_CXX_FLAGS}")

# Add the include directories.
include_directories(src/include)
include_directories(src/intervals)
include_directories(include)

# Add the source files.
add_library(heappulse SHARED
    src/heappulse.cpp)
set_target_properties(heappulse PROPERTIES LINKER_LANGUAGE CXX)
# Link -lz to the library.
target_link_libraries(heappulse z)
# Add -DBKMALLOC_HOOK for heap pulse.
target_compile_definitions(heappulse PRIVATE BKMALLOC_HOOK)


# Add the bkmalloc library.
add_library(bkmalloc SHARED
    include/bkmalloc.cpp)

target_link_libraries(bkmalloc ${CMAKE_DL_LIBS})
