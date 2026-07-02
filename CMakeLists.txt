cmake_minimum_required(VERSION 3.16)
project(module)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/utf-8 /Zi /Od /Ob0)
    add_link_options(/DEBUG /PDB:module.pdb)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC")
endif()


find_library(VORTEX_SHARED_LIBRARY vortex_shared HINTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/build/prod/)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/lib/spdlog/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/lib/cherry/lib/imgui")

if(NOT VORTEX_SHARED_LIBRARY)
    message(FATAL_ERROR "Please build/install the vortex core before compiling modules!")
endif()

file(GLOB_RECURSE SOURCES main/*.cpp)
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})

add_library(module SHARED ${SOURCES})
if(UNIX AND NOT APPLE)
target_compile_options(module PRIVATE -Wall -Wextra)
endif()

target_link_libraries(module PRIVATE ${VORTEX_SHARED_LIBRARY})
target_include_directories(module PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/
${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/vxcore/include/
${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/lib/cherry
)

if(WIN32)
    target_compile_definitions(module PRIVATE
        CHERRY_IMPORT_DLL
        CHERRY_MULTIAPP
    )

    find_library(CHERRY_IMPORT_LIB cherry 
        HINTS 
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/build/prod/
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/build/prod/Release
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/vortex/build/prod/Debug
    )
    if(NOT CHERRY_IMPORT_LIB)
        message(FATAL_ERROR "cherry.lib not builded, please build Vortex before")
    endif()
    target_link_libraries(module PRIVATE ${CHERRY_IMPORT_LIB})
endif()

file(GLOB_RECURSE ASSET_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/main/assets/*"
)

# Configure per-config output directories
foreach(OUTPUTCONFIG Debug Release RelWithDebInfo MinSizeRel)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin/${OUTPUTCONFIG})
endforeach()

if(WIN32)
set_target_properties(module PROPERTIES
    COMPILE_PDB_NAME "module"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
endif()

# Script to remove existing resources
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/remove_resources_if_exists.cmake
"if(EXISTS \"${CMAKE_BINARY_DIR}/bin/$<CONFIG>/resources\")\n"
"    file(REMOVE_RECURSE \"${CMAKE_BINARY_DIR}/bin/$<CONFIG>/resources\")\n"
"endif()\n"
)

# Custom target to sync assets (and pdb files for Windows)
if(WIN32)
add_custom_target(sync_assets_editor ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Synchronizing assets..."
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/remove_resources_if_exists.cmake
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/main/assets
        $<TARGET_FILE_DIR:module>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE_DIR:module>/module.pdb
        $<TARGET_FILE_DIR:module>/module.pdb
    COMMAND ${CMAKE_COMMAND} -E echo "Assets synchronized."
    DEPENDS ${ASSET_FILES} module
)
else()
add_custom_target(sync_assets_editor ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Synchronizing assets..."
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/remove_resources_if_exists.cmake
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/main/assets
        $<TARGET_FILE_DIR:module>/
    COMMAND ${CMAKE_COMMAND} -E echo "Assets synchronized."
    DEPENDS ${ASSET_FILES} module
)
endif()