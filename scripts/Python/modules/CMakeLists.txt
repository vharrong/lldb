# build the Python readline suppression module only on Linux
if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND NOT ANDROID)
   add_subdirectory(readline)
endif()
