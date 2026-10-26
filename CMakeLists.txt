cmake_minimum_required(VERSION 3.16)
project(m_mdy_m_log VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(BUILD_SHARED_LIBS "Build the shared library (OFF builds static)" ON)
option(M_MDY_M_LOG_BUILD_TESTS "Build unit and concurrency tests" ON)
option(M_MDY_M_LOG_BUILD_EXAMPLES "Build C examples" ON)

add_library(m_mdy_m_log src/log.c)
add_library(m-mdy-m::log ALIAS m_mdy_m_log)
set_target_properties(m_mdy_m_log PROPERTIES OUTPUT_NAME "m-mdy-m-log" EXPORT_NAME log)
target_compile_features(m_mdy_m_log PUBLIC c_std_11)
target_include_directories(m_mdy_m_log
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(m_mdy_m_log PRIVATE M_MDY_M_LOG_BUILD)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(m_mdy_m_log PUBLIC M_MDY_M_LOG_SHARED)
  set_target_properties(m_mdy_m_log PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
endif()
if(MSVC)
  target_compile_options(m_mdy_m_log PRIVATE /W4)
else()
  target_compile_options(m_mdy_m_log PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes)
endif()

if(M_MDY_M_LOG_BUILD_TESTS AND BUILD_TESTING)
  add_executable(m_mdy_m_log_tests tests/test_log.c)
  target_link_libraries(m_mdy_m_log_tests PRIVATE m_mdy_m_log)
  add_test(NAME unit-contract COMMAND m_mdy_m_log_tests)
  if(UNIX)
    find_package(Threads REQUIRED)
    add_executable(m_mdy_m_log_threaded_tests tests/test_threaded.c)
    target_link_libraries(m_mdy_m_log_threaded_tests PRIVATE m_mdy_m_log Threads::Threads)
    add_test(NAME concurrent-writes-8000 COMMAND m_mdy_m_log_threaded_tests)
  endif()
endif()

if(M_MDY_M_LOG_BUILD_EXAMPLES)
  add_executable(m_mdy_m_log_example_basic examples/c/basic.c)
  target_link_libraries(m_mdy_m_log_example_basic PRIVATE m_mdy_m_log)
  add_executable(m_mdy_m_log_example_abi_record examples/c/abi_record.c)
  target_link_libraries(m_mdy_m_log_example_abi_record PRIVATE m_mdy_m_log)
  if(BUILD_TESTING)
    add_test(NAME record-abi-example COMMAND m_mdy_m_log_example_abi_record)
  endif()
endif()

set(VERSION "${PROJECT_VERSION}")
configure_file(packaging/m-mdy-m-log.pc.in m-mdy-m-log.pc @ONLY)
configure_package_config_file(
  cmake/m-mdy-m-log-config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/m-mdy-m-log-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/m-mdy-m-log")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/m-mdy-m-log-config-version.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)
install(TARGETS m_mdy_m_log EXPORT m_mdy_m_logTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/m-mdy-m/log.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/m-mdy-m)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/m-mdy-m-log.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(EXPORT m_mdy_m_logTargets
  FILE m-mdy-m-log-targets.cmake
  NAMESPACE m-mdy-m::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/m-mdy-m-log")
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/m-mdy-m-log-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/m-mdy-m-log-config-version.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/m-mdy-m-log")
