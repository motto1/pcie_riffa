# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\pice_dll_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\pice_dll_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\test_pice_dll_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\test_pice_dll_autogen.dir\\ParseCache.txt"
  "pice_dll_autogen"
  "test_pice_dll_autogen"
  )
endif()
