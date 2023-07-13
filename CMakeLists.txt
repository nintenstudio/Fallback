cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(Fallback CXX)
add_library(Fallback STATIC "Public/Fallback/shared_ptr_atomic.h" "Private/dummy.cpp")
set_target_properties(Fallback PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(Fallback PRIVATE Private)
target_include_directories(Fallback PUBLIC Public)

try_compile(FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED ${CMAKE_BINARY_DIR}/FeatureTests ${CMAKE_CURRENT_LIST_DIR}/FeatureTests/shared_ptr_atomic.cpp)
if(FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED)
	target_compile_definitions(Fallback PUBLIC FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED=true)
else()
	target_compile_definitions(Fallback PUBLIC FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED=false)
endif()
message(STATUS "FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED=${FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED}")
target_compile_definitions(Fallback PUBLIC FALLBACK_SHARED_PTR_ATOMIC_SUPPORTED=true)