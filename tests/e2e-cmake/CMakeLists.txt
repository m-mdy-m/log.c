cmake_minimum_required(VERSION 3.16)
project(m_mdy_m_log_external_consumer LANGUAGES C)

find_package(m-mdy-m-log CONFIG REQUIRED)
add_executable(m_mdy_m_log_cmake_consumer ../e2e_consumer.c)
target_link_libraries(m_mdy_m_log_cmake_consumer PRIVATE m-mdy-m::log)
enable_testing()

# The package is installed under a temporary prefix, so the dynamic loader
# must be told where its shared library lives. Keep the caller's environment
# intact and prepend only the staged install path.
if(WIN32)
  add_test(NAME installed-cmake-consumer
    COMMAND ${CMAKE_COMMAND} -E env
      "PATH=${CMAKE_PREFIX_PATH}/bin;$ENV{PATH}"
      $<TARGET_FILE:m_mdy_m_log_cmake_consumer>)
elseif(APPLE)
  add_test(NAME installed-cmake-consumer
    COMMAND ${CMAKE_COMMAND} -E env
      "DYLD_LIBRARY_PATH=${CMAKE_PREFIX_PATH}/lib:$ENV{DYLD_LIBRARY_PATH}"
      $<TARGET_FILE:m_mdy_m_log_cmake_consumer>)
elseif(UNIX)
  add_test(NAME installed-cmake-consumer
    COMMAND ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=${CMAKE_PREFIX_PATH}/lib:$ENV{LD_LIBRARY_PATH}"
      $<TARGET_FILE:m_mdy_m_log_cmake_consumer>)
else()
  add_test(NAME installed-cmake-consumer COMMAND m_mdy_m_log_cmake_consumer)
endif()
