if( DEFINED CMAKE_CROSSCOMPILING )
 # subsequent toolchain loading is not really needed
 return()
endif()

get_property( IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE )
if( IS_IN_TRY_COMPILE )
 # this seems necessary and works fine but I'm unsure if it breaks anything  
 return()
endif()

set( CMAKE_SYSTEM_NAME Linux )

set( ANDROID_ABI "${ANDROID_ABI}" CACHE INTERNAL "Android Abi" FORCE )
if( ANDROID_ABI STREQUAL "x86" )
 set( X86 true )
 set( CMAKE_SYSTEM_PROCESSOR "i686" )
 set( ANDROID_TOOLCHAIN_NAME "x86-linux-android" )
elseif( ANDROID_ABI STREQUAL "x86_64" )
 set( X86 true )
 set( CMAKE_SYSTEM_PROCESSOR "x86_64" ) 
 set( ANDROID_TOOLCHAIN_NAME "x86_64-linux-android" )
else()
 message( SEND_ERROR "Unknown ANDROID_ABI = \"${ANDROID_ABI}\"." )
endif()

get_filename_component( ANDROID_TOOLCHAIN "${ANDROID_TOOLCHAIN}" ABSOLUTE )
set( ANDROID_TOOLCHAIN "${ANDROID_TOOLCHAIN}" CACHE INTERNAL "Android standalone toolchain" FORCE )
set( ANDROID_SYSROOT "${ANDROID_TOOLCHAIN}/sysroot" CACHE INTERNAL "Android Sysroot" FORCE )

if( NOT CMAKE_C_COMPILER )
 set( CMAKE_C_COMPILER   "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-gcc"     CACHE PATH "C compiler" )
 set( CMAKE_CXX_COMPILER "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-g++"     CACHE PATH "C++ compiler" )
 set( CMAKE_ASM_COMPILER "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-gcc"     CACHE PATH "assembler" )
 set( CMAKE_STRIP        "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-strip"   CACHE PATH "strip" )
 set( CMAKE_AR           "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-ar"      CACHE PATH "archive" )
 set( CMAKE_LINKER       "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-ld"      CACHE PATH "linker" )
 set( CMAKE_NM           "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-nm"      CACHE PATH "nm" )
 set( CMAKE_OBJCOPY      "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-objcopy" CACHE PATH "objcopy" )
 set( CMAKE_OBJDUMP      "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-objdump" CACHE PATH "objdump" )
 set( CMAKE_RANLIB       "${ANDROID_TOOLCHAIN}/bin/${ANDROID_TOOLCHAIN_NAME}-ranlib"  CACHE PATH "ranlib" )
endif()

include( CMakeForceCompiler )
# flags and definitions
remove_definitions( -DANDROID -D__ANDROID__ )
#add_definitions( -DANDROID -D__ANDROID__ )
add_definitions( -DANDROID -D__ANDROID_NDK__ -DLLDB_DISABLE_LIBEDIT )

set( ANDROID_CXX_FLAGS "--sysroot=${ANDROID_SYSROOT} -pie -fPIE -funwind-tables -fsigned-char -no-canonical-prefixes" )
# TODO: different ARM abi have different flags such as neon, vfpv etc
if( X86 )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -funswitch-loops -finline-limit=300" )
endif()

# linker flags
set( ANDROID_CXX_FLAGS    "${ANDROID_CXX_FLAGS} -fdata-sections -ffunction-sections" )
set( ANDROID_LINKER_FLAGS "${ANDROID_LINKER_FLAGS} -Wl,--gc-sections" )

# cache flags
set( CMAKE_CXX_FLAGS           ""                        CACHE STRING "c++ flags" )
set( CMAKE_C_FLAGS             ""                        CACHE STRING "c flags" )
set( CMAKE_EXE_LINKER_FLAGS    "-Wl,-z,nocopyreloc"      CACHE STRING "executable linker flags" )
set( ANDROID_CXX_FLAGS         "${ANDROID_CXX_FLAGS}"    CACHE INTERNAL "Android c/c++ flags" )
set( ANDROID_LINKER_FLAGS      "${ANDROID_LINKER_FLAGS}" CACHE INTERNAL "Android c/c++ linker flags" )

# final flags
set( CMAKE_CXX_FLAGS           "${ANDROID_CXX_FLAGS} ${CMAKE_CXX_FLAGS}" )
set( CMAKE_C_FLAGS             "${ANDROID_CXX_FLAGS} ${CMAKE_C_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS    "${ANDROID_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}" )

# global includes and link directories
set( ANDROID_INCLUDE_DIRS "${ANDROID_TOOLCHAIN}/include/c++/${ANDROID_COMPILER_VERSION}" )
if( ARMEABI_V7A AND EXISTS "${ANDROID_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_NAME}/${CMAKE_SYSTEM_PROCESSOR}/bits" )
 list( APPEND ANDROID_INCLUDE_DIRS "${ANDROID_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_NAME}/${CMAKE_SYSTEM_PROCESSOR}" )
elseif( ARMEABI AND NOT ANDROID_FORCE_ARM_BUILD AND EXISTS "${ANDROID_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_NAME}/thumb/bits" )
 list( APPEND ANDROID_INCLUDE_DIRS "${ANDROID_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_NAME}/thumb" )
else()
 list( APPEND ANDROID_INCLUDE_DIRS "${ANDROID_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_NAME}" )
endif()
list( APPEND ANDROID_INCLUDE_DIRS "${ANDROID_TOOLCHAIN}/include/python2.7" )
include_directories( SYSTEM "${ANDROID_SYSROOT}/usr/include" ${ANDROID_INCLUDE_DIRS} )

# set these global flags for cmake client scripts to change behavior
set( ANDROID True )
set( BUILD_ANDROID True )
set( __ANDROID_NDK__ True )

# where is the target environment
set( CMAKE_FIND_ROOT_PATH "${ANDROID_TOOLCHAIN}/bin" "${ANDROID_TOOLCHAIN}/${ANDROID_TOOLCHAIN_NAME}" "${ANDROID_SYSROOT}" "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_PREFIX}/share" )

# only search for libraries and includes in the ndk toolchain
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )