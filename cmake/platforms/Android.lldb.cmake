get_property( _CMAKE_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE )
if( _CMAKE_IN_TRY_COMPILE )
 return()
endif()

# this one is important
set( CMAKE_SYSTEM_NAME Linux )

set( ANDROID_SUPPORTED_ABIS_arm "armeabi-v7a;armeabi;armeabi-v7a with NEON;armeabi-v7a with VFPV3;armeabi-v6 with VFP" )
set( ANDROID_SUPPORTED_ABIS_x86 "x86" )
set( ANDROID_SUPPORTED_ABIS_x86_64 "x86_64" )
set( ANDROID_SUPPORTED_ABIS_mipsel "mips" )

set( ANDROID_DEFAULT_NDK_API_LEVEL 8 )
set( ANDROID_DEFAULT_NDK_API_LEVEL_x86 9 )
set( ANDROID_DEFAULT_NDK_API_LEVEL_x86_64 L )
set( ANDROID_DEFAULT_NDK_API_LEVEL_mips 9 )

macro( __LIST_FILTER listvar regex )
 if( ${listvar} )
  foreach( __val ${${listvar}} )
   if( __val MATCHES "${regex}" )
    list( REMOVE_ITEM ${listvar} "${__val}" )
   endif()
  endforeach()
 endif()
endmacro()

macro( __INIT_VARIABLE var_name )
 set( __test_path 0 )
 foreach( __var ${ARGN} )
  if( __var STREQUAL "PATH" )
   set( __test_path 1 )
   break()
  endif()
 endforeach()
 if( __test_path AND NOT EXISTS "${${var_name}}" )
  unset( ${var_name} CACHE )
 endif()
 if( "${${var_name}}" STREQUAL "" )
  set( __values 0 )
  foreach( __var ${ARGN} )
   if( __var STREQUAL "VALUES" )
    set( __values 1 )
   elseif( NOT __var STREQUAL "PATH" )
    set( __obsolete 0 )
    if( __var MATCHES "^OBSOLETE_.*$" )
     string( REPLACE "OBSOLETE_" "" __var "${__var}" )
     set( __obsolete 1 )
    endif()
    if( __var MATCHES "^ENV_.*$" )
     string( REPLACE "ENV_" "" __var "${__var}" )
     set( __value "$ENV{${__var}}" )
    elseif( DEFINED ${__var} )
     set( __value "${${__var}}" )
    else()
     if( __values )
      set( __value "${__var}" )
     else()
      set( __value "" )
     endif()
    endif()
    if( NOT "${__value}" STREQUAL "" )
     if( __test_path )
      if( EXISTS "${__value}" )
       file( TO_CMAKE_PATH "${__value}" ${var_name} )
       if( __obsolete AND NOT _CMAKE_IN_TRY_COMPILE )
        message( WARNING "Using value of obsolete variable ${__var} as initial value for ${var_name}. Please note, that ${__var} can be completely removed in future versions of the toolchain." )
       endif()
       break()
      endif()
     else()
      set( ${var_name} "${__value}" )
       if( __obsolete AND NOT _CMAKE_IN_TRY_COMPILE )
        message( WARNING "Using value of obsolete variable ${__var} as initial value for ${var_name}. Please note, that ${__var} can be completely removed in future versions of the toolchain." )
       endif()
      break()
     endif()
    endif()
   endif()
  endforeach()
  unset( __value )
  unset( __values )
  unset( __obsolete )
 elseif( __test_path )
  file( TO_CMAKE_PATH "${${var_name}}" ${var_name} )
 endif()
 unset( __test_path )
endmacro()

macro( __DETECT_NATIVE_API_LEVEL _var _path )
 #SET( __ndkApiLevelRegex "^[\t ]*#define[\t ]+__ANDROID_API__[\t ]+([0-9]+)[\t ]*$" ) 
 SET( __ndkApiLevelRegex "[\t ]*#define[\t ]+__ANDROID_API__[\t ]+([0-9]+)[\t ]*" )
 FILE( STRINGS ${_path} __apiFileContent REGEX "${__ndkApiLevelRegex}" ) 
 if( NOT __apiFileContent )
  message( SEND_ERROR "Could not get Android native API level. Probably you have specified invalid level value, or your copy of NDK/toolchain is broken." )
 endif()
 string( REGEX REPLACE "${__ndkApiLevelRegex}" "\\1" ${_var} "${__apiFileContent}" )
 #message( WARNING "${__apiFileContent}")
 #message( WARNING "${_var}")
 unset( __apiFileContent )
 unset( __ndkApiLevelRegex )
endmacro()

macro( __DETECT_TOOLCHAIN_MACHINE_NAME _var _root )
 if( EXISTS "${_root}" )
  file( GLOB __gccExePath RELATIVE "${_root}/bin/" "${_root}/bin/*-gcc${TOOL_OS_SUFFIX}" )
  __LIST_FILTER( __gccExePath "^[.].*" )
  list( LENGTH __gccExePath __gccExePathsCount )
  if( NOT __gccExePathsCount EQUAL 1  AND NOT _CMAKE_IN_TRY_COMPILE )
   message( WARNING "Could not determine machine name for compiler from ${_root}" )
   set( ${_var} "" )
  else()
   get_filename_component( __gccExeName "${__gccExePath}" NAME_WE )
   string( REPLACE "-gcc" "" ${_var} "${__gccExeName}" )
  endif()
  unset( __gccExePath )
  unset( __gccExePathsCount )
  unset( __gccExeName )
 else()
  set( ${_var} "" )
 endif()
endmacro()

# detect current host platform
if( NOT DEFINED ANDROID_NDK_HOST_X64 AND (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "amd64|x86_64|AMD64" OR CMAKE_HOST_APPLE) )
 set( ANDROID_NDK_HOST_X64 1 CACHE BOOL "Try to use 64-bit compiler toolchain" )
 mark_as_advanced( ANDROID_NDK_HOST_X64 )
endif()

set( TOOL_OS_SUFFIX "" )
if( CMAKE_HOST_APPLE )
 set( ANDROID_NDK_HOST_SYSTEM_NAME "darwin-x86_64" )
 set( ANDROID_NDK_HOST_SYSTEM_NAME2 "darwin-x86" )
elseif( CMAKE_HOST_WIN32 )
 set( ANDROID_NDK_HOST_SYSTEM_NAME "windows-x86_64" )
 set( ANDROID_NDK_HOST_SYSTEM_NAME2 "windows" )
 set( TOOL_OS_SUFFIX ".exe" )
elseif( CMAKE_HOST_UNIX )
 set( ANDROID_NDK_HOST_SYSTEM_NAME "linux-x86_64" )
 set( ANDROID_NDK_HOST_SYSTEM_NAME2 "linux-x86" )
else()
 message( FATAL_ERROR "Cross-compilation on your platform is not supported by this cmake toolchain" )
endif()

if( NOT ANDROID_NDK_HOST_X64 )
 set( ANDROID_NDK_HOST_SYSTEM_NAME ${ANDROID_NDK_HOST_SYSTEM_NAME2} )
endif()

if( ANDROID_STANDALONE_TOOLCHAIN )
 get_filename_component( ANDROID_STANDALONE_TOOLCHAIN "${ANDROID_STANDALONE_TOOLCHAIN}" ABSOLUTE )
 # try to detect change
 if( CMAKE_AR )
  string( LENGTH "${ANDROID_STANDALONE_TOOLCHAIN}" __length )
  string( SUBSTRING "${CMAKE_AR}" 0 ${__length} __androidStandaloneToolchainPreviousPath )
  if( NOT __androidStandaloneToolchainPreviousPath STREQUAL ANDROID_STANDALONE_TOOLCHAIN )
   message( FATAL_ERROR "It is not possible to change path to the Android standalone toolchain on subsequent run." )
  endif()
  unset( __androidStandaloneToolchainPreviousPath )
  unset( __length )
 endif()
 set( ANDROID_STANDALONE_TOOLCHAIN "${ANDROID_STANDALONE_TOOLCHAIN}" CACHE INTERNAL "Path of the Android standalone toolchain" FORCE )
 set( BUILD_WITH_STANDALONE_TOOLCHAIN True )
else()
 message( FATAL_ERROR "only standalone toolchain is supported" )
endif()

# get all the details about standalone toolchain
if( BUILD_WITH_STANDALONE_TOOLCHAIN )
 __DETECT_NATIVE_API_LEVEL( ANDROID_SUPPORTED_NATIVE_API_LEVELS "${ANDROID_STANDALONE_TOOLCHAIN}/sysroot/usr/include/android/api-level.h" )
 set( ANDROID_STANDALONE_TOOLCHAIN_API_LEVEL ${ANDROID_SUPPORTED_NATIVE_API_LEVELS} )
 set( __availableToolchains "standalone" )
 __DETECT_TOOLCHAIN_MACHINE_NAME( __availableToolchainMachines "${ANDROID_STANDALONE_TOOLCHAIN}" )
 if( NOT __availableToolchainMachines )
  message( FATAL_ERROR "Could not determine machine name of your toolchain. Probably your Android standalone toolchain is broken." )
 endif()
 if( __availableToolchainMachines MATCHES i686 )
  set( __availableToolchainArchs "x86" )
 elseif( __availableToolchainMachines MATCHES x86_64 )
  set( __availableToolchainArchs "x86_64" )
 elseif( __availableToolchainMachines MATCHES arm )
  set( __availableToolchainArchs "arm" )
 elseif( __availableToolchainMachines MATCHES mipsel )
  set( __availableToolchainArchs "mipsel" )
 endif()
 execute_process( COMMAND "${ANDROID_STANDALONE_TOOLCHAIN}/bin/${__availableToolchainMachines}-gcc${TOOL_OS_SUFFIX}" -dumpversion
                  OUTPUT_VARIABLE __availableToolchainCompilerVersions OUTPUT_STRIP_TRAILING_WHITESPACE )
 string( REGEX MATCH "[0-9]+[.][0-9]+([.][0-9]+)?" __availableToolchainCompilerVersions "${__availableToolchainCompilerVersions}" )
 if( EXISTS "${ANDROID_STANDALONE_TOOLCHAIN}/bin/clang${TOOL_OS_SUFFIX}" )
  list( APPEND __availableToolchains "standalone-clang" )
  list( APPEND __availableToolchainMachines ${__availableToolchainMachines} )
  list( APPEND __availableToolchainArchs ${__availableToolchainArchs} )
  list( APPEND __availableToolchainCompilerVersions ${__availableToolchainCompilerVersions} )
 endif()
endif()

macro( __GLOB_NDK_TOOLCHAINS __availableToolchainsVar __availableToolchainsLst __toolchain_subpath )
 foreach( __toolchain ${${__availableToolchainsLst}} )
  if( "${__toolchain}" MATCHES "-clang3[.][0-9]$" AND NOT EXISTS "${ANDROID_NDK_TOOLCHAINS_PATH}/${__toolchain}${__toolchain_subpath}" )
   string( REGEX REPLACE "-clang3[.][0-9]$" "-4.6" __gcc_toolchain "${__toolchain}" )
  else()
   set( __gcc_toolchain "${__toolchain}" )
  endif()
  __DETECT_TOOLCHAIN_MACHINE_NAME( __machine "${ANDROID_NDK_TOOLCHAINS_PATH}/${__gcc_toolchain}${__toolchain_subpath}" )
  if( __machine )
   string( REGEX MATCH "[0-9]+[.][0-9]+([.][0-9x]+)?$" __version "${__gcc_toolchain}" )
   if( __machine MATCHES i686 )
    set( __arch "x86" )
   elseif( __machine MATCHES arm )
    set( __arch "arm" )
   elseif( __machine MATCHES mipsel )
    set( __arch "mipsel" )
   endif()
   list( APPEND __availableToolchainMachines "${__machine}" )
   list( APPEND __availableToolchainArchs "${__arch}" )
   list( APPEND __availableToolchainCompilerVersions "${__version}" )
   list( APPEND ${__availableToolchainsVar} "${__toolchain}" )
  endif()
  unset( __gcc_toolchain )
 endforeach()
endmacro()

# build list of available ABIs
set( ANDROID_SUPPORTED_ABIS "" )
set( __uniqToolchainArchNames ${__availableToolchainArchs} )
list( REMOVE_DUPLICATES __uniqToolchainArchNames )
list( SORT __uniqToolchainArchNames )
foreach( __arch ${__uniqToolchainArchNames} )
 list( APPEND ANDROID_SUPPORTED_ABIS ${ANDROID_SUPPORTED_ABIS_${__arch}} )
endforeach()
unset( __uniqToolchainArchNames )
if( NOT ANDROID_SUPPORTED_ABIS )
 message( FATAL_ERROR "No one of known Android ABIs is supported by this cmake toolchain." )
endif()

# choose target ABI
__INIT_VARIABLE( ANDROID_ABI OBSOLETE_ARM_TARGET OBSOLETE_ARM_TARGETS VALUES ${ANDROID_SUPPORTED_ABIS} )
# verify that target ABI is supported
list( FIND ANDROID_SUPPORTED_ABIS "${ANDROID_ABI}" __androidAbiIdx )
if( __androidAbiIdx EQUAL -1 )
 string( REPLACE ";" "\", \"" PRINTABLE_ANDROID_SUPPORTED_ABIS  "${ANDROID_SUPPORTED_ABIS}" )
 message( FATAL_ERROR "Specified ANDROID_ABI = \"${ANDROID_ABI}\" is not supported by this cmake toolchain or your NDK/toolchain.
   Supported values are: \"${PRINTABLE_ANDROID_SUPPORTED_ABIS}\"
   " )
endif()
unset( __androidAbiIdx )

# set target ABI options
if( ANDROID_ABI STREQUAL "x86" )
 set( X86 true )
 set( ANDROID_NDK_ABI_NAME "x86" )
 set( ANDROID_ARCH_NAME "x86" )
 set( ANDROID_ARCH_FULLNAME "x86" )
 set( ANDROID_LLVM_TRIPLE "i686-none-linux-android" )
 set( CMAKE_SYSTEM_PROCESSOR "i686" )
elseif( ANDROID_ABI STREQUAL "x86_64" )
 set( X86 true )
 set( ANDROID_NDK_ABI_NAME "x86_64" )
 set( ANDROID_ARCH_NAME "x86_64" )
 set( ANDROID_ARCH_FULLNAME "x86_64" )
 set( ANDROID_LLVM_TRIPLE "x86_64-linux-android" )
 set( CMAKE_SYSTEM_PROCESSOR "x86_64" ) 
elseif( ANDROID_ABI STREQUAL "mips" )
 set( MIPS true )
 set( ANDROID_NDK_ABI_NAME "mips" )
 set( ANDROID_ARCH_NAME "mips" )
 set( ANDROID_ARCH_FULLNAME "mipsel" )
 set( ANDROID_LLVM_TRIPLE "mipsel-none-linux-android" )
 set( CMAKE_SYSTEM_PROCESSOR "mips" )
elseif( ANDROID_ABI STREQUAL "armeabi" )
 set( ARMEABI true )
 set( ANDROID_NDK_ABI_NAME "armeabi" )
 set( ANDROID_ARCH_NAME "arm" )
 set( ANDROID_ARCH_FULLNAME "arm" )
 set( ANDROID_LLVM_TRIPLE "armv5te-none-linux-androideabi" )
 set( CMAKE_SYSTEM_PROCESSOR "armv5te" )
elseif( ANDROID_ABI STREQUAL "armeabi-v6 with VFP" )
 set( ARMEABI_V6 true )
 set( ANDROID_NDK_ABI_NAME "armeabi" )
 set( ANDROID_ARCH_NAME "arm" )
 set( ANDROID_ARCH_FULLNAME "arm" )
 set( ANDROID_LLVM_TRIPLE "armv5te-none-linux-androideabi" )
 set( CMAKE_SYSTEM_PROCESSOR "armv6" )
 # need always fallback to older platform
 set( ARMEABI true )
elseif( ANDROID_ABI STREQUAL "armeabi-v7a")
 set( ARMEABI_V7A true )
 set( ANDROID_NDK_ABI_NAME "armeabi-v7a" )
 set( ANDROID_ARCH_NAME "arm" )
 set( ANDROID_ARCH_FULLNAME "arm" )
 set( ANDROID_LLVM_TRIPLE "armv7-none-linux-androideabi" )
 set( CMAKE_SYSTEM_PROCESSOR "armv7-a" )
elseif( ANDROID_ABI STREQUAL "armeabi-v7a with VFPV3" )
 set( ARMEABI_V7A true )
 set( ANDROID_NDK_ABI_NAME "armeabi-v7a" )
 set( ANDROID_ARCH_NAME "arm" )
 set( ANDROID_ARCH_FULLNAME "arm" )
 set( ANDROID_LLVM_TRIPLE "armv7-none-linux-androideabi" )
 set( CMAKE_SYSTEM_PROCESSOR "armv7-a" )
 set( VFPV3 true )
elseif( ANDROID_ABI STREQUAL "armeabi-v7a with NEON" )
 set( ARMEABI_V7A true )
 set( ANDROID_NDK_ABI_NAME "armeabi-v7a" )
 set( ANDROID_ARCH_NAME "arm" )
 set( ANDROID_ARCH_FULLNAME "arm" )
 set( ANDROID_LLVM_TRIPLE "armv7-none-linux-androideabi" )
 set( CMAKE_SYSTEM_PROCESSOR "armv7-a" )
 set( VFPV3 true )
 set( NEON true )
else()
 message( SEND_ERROR "Unknown ANDROID_ABI=\"${ANDROID_ABI}\" is specified." )
endif()

# choose toolchain
if( ANDROID_TOOLCHAIN_NAME )
 list( FIND __availableToolchains "${ANDROID_TOOLCHAIN_NAME}" __toolchainIdx )
 if( __toolchainIdx EQUAL -1 )
  list( SORT __availableToolchains )
  string( REPLACE ";" "\n  * " toolchains_list "${__availableToolchains}" )
  set( toolchains_list "  * ${toolchains_list}")
  message( FATAL_ERROR "Specified toolchain \"${ANDROID_TOOLCHAIN_NAME}\" is missing in your NDK or broken. Please verify that your NDK is working or select another compiler toolchain.
To configure the toolchain set CMake variable ANDROID_TOOLCHAIN_NAME to one of the following values:\n${toolchains_list}\n" )
 endif()
 list( GET __availableToolchainArchs ${__toolchainIdx} __toolchainArch )
 if( NOT __toolchainArch STREQUAL ANDROID_ARCH_FULLNAME )
  message( SEND_ERROR "Selected toolchain \"${ANDROID_TOOLCHAIN_NAME}\" is not able to compile binaries for the \"${ANDROID_ARCH_NAME}\" platform." )
 endif()
else()
 set( __toolchainIdx -1 )
 set( __applicableToolchains "" )
 set( __toolchainMaxVersion "0.0.0" )
 list( LENGTH __availableToolchains __availableToolchainsCount )
 math( EXPR __availableToolchainsCount "${__availableToolchainsCount}-1" )
 foreach( __idx RANGE ${__availableToolchainsCount} )
  list( GET __availableToolchainArchs ${__idx} __toolchainArch )
  if( __toolchainArch STREQUAL ANDROID_ARCH_FULLNAME )
   list( GET __availableToolchainCompilerVersions ${__idx} __toolchainVersion )
   string( REPLACE "x" "99" __toolchainVersion "${__toolchainVersion}")
   if( __toolchainVersion VERSION_GREATER __toolchainMaxVersion )
    set( __toolchainMaxVersion "${__toolchainVersion}" )
    set( __toolchainIdx ${__idx} )
   endif()
  endif()
 endforeach()
 unset( __availableToolchainsCount )
 unset( __toolchainMaxVersion )
 unset( __toolchainVersion )
endif()
unset( __toolchainArch )
if( __toolchainIdx EQUAL -1 )
 message( FATAL_ERROR "No one of available compiler toolchains is able to compile for ${ANDROID_ARCH_NAME} platform." )
endif()
list( GET __availableToolchains ${__toolchainIdx} ANDROID_TOOLCHAIN_NAME )
list( GET __availableToolchainMachines ${__toolchainIdx} ANDROID_TOOLCHAIN_MACHINE_NAME )
list( GET __availableToolchainCompilerVersions ${__toolchainIdx} ANDROID_COMPILER_VERSION )

unset( __toolchainIdx )
unset( __availableToolchains )
unset( __availableToolchainMachines )
unset( __availableToolchainArchs )
unset( __availableToolchainCompilerVersions )

set( ANDROID_TOOLCHAIN_ROOT "${ANDROID_STANDALONE_TOOLCHAIN}" )
set( ANDROID_SYSROOT "${ANDROID_STANDALONE_TOOLCHAIN}/sysroot" )

# ccache support
__INIT_VARIABLE( _ndk_ccache NDK_CCACHE ENV_NDK_CCACHE )
if( _ndk_ccache )
 if( DEFINED NDK_CCACHE AND NOT EXISTS NDK_CCACHE )
  unset( NDK_CCACHE CACHE )
 endif()
 find_program( NDK_CCACHE "${_ndk_ccache}" DOC "The path to ccache binary")
else()
 unset( NDK_CCACHE CACHE )
endif()
unset( _ndk_ccache )

# setup the cross-compiler
if( NOT CMAKE_C_COMPILER )
 set( CMAKE_C_COMPILER   "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-gcc${TOOL_OS_SUFFIX}"    CACHE PATH "C compiler" )
 set( CMAKE_CXX_COMPILER "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-g++${TOOL_OS_SUFFIX}"    CACHE PATH "C++ compiler" )
 set( CMAKE_ASM_COMPILER "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-gcc${TOOL_OS_SUFFIX}"     CACHE PATH "assembler" )
 set( CMAKE_STRIP        "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-strip${TOOL_OS_SUFFIX}"   CACHE PATH "strip" )
 set( CMAKE_AR           "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-ar${TOOL_OS_SUFFIX}"      CACHE PATH "archive" )
 set( CMAKE_LINKER       "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-ld${TOOL_OS_SUFFIX}"      CACHE PATH "linker" )
 set( CMAKE_NM           "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-nm${TOOL_OS_SUFFIX}"      CACHE PATH "nm" )
 set( CMAKE_OBJCOPY      "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-objcopy${TOOL_OS_SUFFIX}" CACHE PATH "objcopy" )
 set( CMAKE_OBJDUMP      "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-objdump${TOOL_OS_SUFFIX}" CACHE PATH "objdump" )
 set( CMAKE_RANLIB       "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-ranlib${TOOL_OS_SUFFIX}"  CACHE PATH "ranlib" )
endif()

set( _CMAKE_TOOLCHAIN_PREFIX "${ANDROID_TOOLCHAIN_MACHINE_NAME}-" )
if( CMAKE_VERSION VERSION_LESS 2.8.5 )
 set( CMAKE_ASM_COMPILER_ARG1 "-c" )
endif()

# Force set compilers because standard identification works badly for us
include( CMakeForceCompiler )
CMAKE_FORCE_C_COMPILER( "${CMAKE_C_COMPILER}" GNU )
set( CMAKE_C_PLATFORM_ID Linux )
set( CMAKE_C_SIZEOF_DATA_PTR 4 )
set( CMAKE_C_HAS_ISYSROOT 1 )
set( CMAKE_C_COMPILER_ABI ELF )
CMAKE_FORCE_CXX_COMPILER( "${CMAKE_CXX_COMPILER}" GNU )
set( CMAKE_CXX_SOURCE_FILE_EXTENSIONS cc cp cxx cpp CPP c++ C )

# flags and definitions
remove_definitions( -DANDROID -D__ANDROID__ )
#add_definitions( -DANDROID -D__ANDROID__ )
add_definitions( -DANDROID -D__ANDROID_NDK__ -DLLDB_DISABLE_LIBEDIT )

set( ANDROID_CXX_FLAGS "--sysroot=${ANDROID_SYSROOT}" )

# NDK flags
if( ARMEABI OR ARMEABI_V7A )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -pie -fPIE -fpic -funwind-tables" )
elseif( X86 )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -pie -fPIE -funwind-tables" )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -funswitch-loops -finline-limit=300" )
elseif( MIPS )
 set( ANDROID_CXX_FLAGS         "${ANDROID_CXX_FLAGS} -fpic -fno-strict-aliasing -finline-functions -ffunction-sections -funwind-tables -fmessage-length=0" )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -fno-inline-functions-called-once -fgcse-after-reload -frerun-cse-after-loop -frename-registers" )
 set( ANDROID_CXX_FLAGS_RELEASE "${ANDROID_CXX_FLAGS_RELEASE} -funswitch-loops -finline-limit=300" )
elseif()
 set( ANDROID_CXX_FLAGS_RELEASE "" )
 set( ANDROID_CXX_FLAGS_DEBUG   "" )
endif()

set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -fsigned-char" ) # good/necessary when porting desktop libraries

if( NOT X86 AND NOT ANDROID_COMPILER_IS_CLANG )
 set( ANDROID_CXX_FLAGS "-Wno-psabi ${ANDROID_CXX_FLAGS}" )
endif()

if( NOT ANDROID_COMPILER_VERSION VERSION_LESS "4.6" )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -no-canonical-prefixes" ) # see https://android-review.googlesource.com/#/c/47564/
endif()

# ABI-specific flags
if( ARMEABI_V7A )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -march=armv7-a -mfloat-abi=softfp" )
 if( NEON )
  set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -mfpu=neon" )
 elseif( VFPV3 )
  set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -mfpu=vfpv3" )
 else()
  set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -mfpu=vfpv3-d16" )
 endif()
elseif( ARMEABI_V6 )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -march=armv6 -mfloat-abi=softfp -mfpu=vfp" ) # vfp == vfpv2
elseif( ARMEABI )
 set( ANDROID_CXX_FLAGS "${ANDROID_CXX_FLAGS} -march=armv5te -mtune=xscale -msoft-float" )
endif()

# linker flags
set( ANDROID_LINKER_FLAGS "" )

set( ANDROID_CXX_FLAGS    "${ANDROID_CXX_FLAGS} -fdata-sections -ffunction-sections" )
set( ANDROID_LINKER_FLAGS "${ANDROID_LINKER_FLAGS} -Wl,--gc-sections" )

# cache flags
set( CMAKE_CXX_FLAGS           ""                        CACHE STRING "c++ flags" )
set( CMAKE_C_FLAGS             ""                        CACHE STRING "c flags" )
set( CMAKE_EXE_LINKER_FLAGS    "-Wl,-z,nocopyreloc"      CACHE STRING "executable linker flags" )

# put flags to cache (for debug purpose only)
set( ANDROID_CXX_FLAGS         "${ANDROID_CXX_FLAGS}"         CACHE INTERNAL "Android specific c/c++ flags" )
set( ANDROID_LINKER_FLAGS      "${ANDROID_LINKER_FLAGS}"      CACHE INTERNAL "Android specific c/c++ linker flags" )

# finish flags
set( CMAKE_CXX_FLAGS           "${ANDROID_CXX_FLAGS} ${CMAKE_CXX_FLAGS} -I${ANDROID_TOOLCHAIN_ROOT}/include/python2.7 -fpermissive" )
set( CMAKE_C_FLAGS             "${ANDROID_CXX_FLAGS} ${CMAKE_C_FLAGS}" )
set( CMAKE_EXE_LINKER_FLAGS    "${ANDROID_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}" )

# global includes and link directories
set( ANDROID_ADDITIONAL_INCLUDE_DIRS "${ANDROID_STANDALONE_TOOLCHAIN}/include/c++/${ANDROID_COMPILER_VERSION}" )
if( NOT EXISTS "${ANDROID_ADDITIONAL_INCLUDE_DIRS}" )
 # old location ( pre r8c )
 set( ANDROID_ADDITIONAL_INCLUDE_DIRS "${ANDROID_STANDALONE_TOOLCHAIN}/${ANDROID_TOOLCHAIN_MACHINE_NAME}/include/c++/${ANDROID_COMPILER_VERSION}" )
endif()
if( ARMEABI_V7A AND EXISTS "${ANDROID_ADDITIONAL_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_MACHINE_NAME}/${CMAKE_SYSTEM_PROCESSOR}/bits" )
 list( APPEND ANDROID_ADDITIONAL_INCLUDE_DIRS "${ANDROID_ADDITIONAL_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_MACHINE_NAME}/${CMAKE_SYSTEM_PROCESSOR}" )
elseif( ARMEABI AND NOT ANDROID_FORCE_ARM_BUILD AND EXISTS "${ANDROID_ADDITIONAL_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_MACHINE_NAME}/thumb/bits" )
 list( APPEND ANDROID_ADDITIONAL_INCLUDE_DIRS "${ANDROID_ADDITIONAL_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_MACHINE_NAME}/thumb" )
else()
 list( APPEND ANDROID_ADDITIONAL_INCLUDE_DIRS "${ANDROID_ADDITIONAL_INCLUDE_DIRS}/${ANDROID_TOOLCHAIN_MACHINE_NAME}" )
endif()

include_directories( SYSTEM "${ANDROID_SYSROOT}/usr/include" ${ANDROID_STL_INCLUDE_DIRS} ${ANDROID_ADDITIONAL_INCLUDE_DIRS} )
get_filename_component(__android_install_path "${CMAKE_INSTALL_PREFIX}/libs/${ANDROID_NDK_ABI_NAME}" ABSOLUTE) # avoid CMP0015 policy warning
link_directories( "${__android_install_path}" )

# set these global flags for cmake client scripts to change behavior
set( ANDROID True )
set( BUILD_ANDROID True )
set( __ANDROID_NDK__ True )

# where is the target environment
set( CMAKE_FIND_ROOT_PATH "${ANDROID_TOOLCHAIN_ROOT}/bin" "${ANDROID_TOOLCHAIN_ROOT}/${ANDROID_TOOLCHAIN_MACHINE_NAME}" "${ANDROID_SYSROOT}" "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_PREFIX}/share" )

# only search for libraries and includes in the ndk toolchain
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )