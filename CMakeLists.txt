cmake_minimum_required(VERSION 3.12)

project(sigslot LANGUAGES CXX VERSION 1.0)

# Library type: STATIC, SHARED, INTERFACE
set(SIGSLOT_LIBRARY_TYPE "STATIC" CACHE STRING "Library type: STATIC, SHARED or INTERFACE")
set_property(CACHE SIGSLOT_LIBRARY_TYPE PROPERTY STRINGS "STATIC" "SHARED" "INTERFACE")

set(SIGSLOT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/sigslot.h)
set(SIGSLOT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/sigslot.cc)

if(SIGSLOT_LIBRARY_TYPE STREQUAL "INTERFACE")
  add_library(sigslot INTERFACE)
  target_include_directories(sigslot INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
else()
  add_library(sigslot ${SIGSLOT_SOURCES} ${SIGSLOT_HEADERS})
  if(SIGSLOT_LIBRARY_TYPE STREQUAL "SHARED")
    set_target_properties(sigslot PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
  target_include_directories(sigslot PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include>)
  target_compile_features(sigslot PUBLIC cxx_std_11)
  set_target_properties(sigslot PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

option(SIGSLOT_INSTALL "Generate install targets" ON)
if(SIGSLOT_INSTALL)
  install(FILES ${SIGSLOT_HEADERS} DESTINATION include)
  if(NOT SIGSLOT_LIBRARY_TYPE STREQUAL "INTERFACE")
    install(TARGETS sigslot
      EXPORT sigslotTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin
    )
    install(EXPORT sigslotTargets FILE sigslotTargets.cmake NAMESPACE sigslot:: DESTINATION lib/cmake/sigslot)
  else()
    install(TARGETS sigslot EXPORT sigslotTargets FILE sigslotTargets.cmake NAMESPACE sigslot:: DESTINATION lib/cmake/sigslot)
  endif()
endif()

add_library(sigslot::sigslot ALIAS sigslot)
