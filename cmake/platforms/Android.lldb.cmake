if( DEFINED CMAKE_CROSSCOMPILING )
 # subsequent toolchain loading is not really needed
 return()
endif()

get_property( IS_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE )
if( IS_IN_TRY_COMPILE )
 # this seems necessary but  
 return()
endif()

# this one is important
set( CMAKE_SYSTEM_NAME Linux )

# set target ABI options
set( ANDROID_ABI "${ANDROID_ABI}" CACHE INTERNAL "Android Abi" FORCE )
message( STATUS "abi = ${ANDROID_ABI} try_compile=${_CMAKE_IN_TRY_COMPILE}" )
if( ANDROID_ABI STREQUAL "x86" )
 set( X86 true )
 set( ANDROID_NDK_ABI_NAME "x86" )
 set( ANDROID_ARCH_NAME "x86" )
 set( ANDROID_ARCH_FULLNAME "x86" )
 set( ANDROID_LLVM_TRIPLE "i686-none-linux-android" )
 set( CMAKE_SYSTEM_PROCESSOR "i686" )
 set( ANDROID_TOOLCHAIN_MACHINE_NAME "x86-linux-android" )
elseif( ANDROID_ABI STREQUAL "x86_64" )
 message( STATUS "here")
 set( X86 true )
 set( ANDROID_NDK_ABI_NAME "X64" )
 set( ANDROID_ARCH_NAME "x86_64" )
 set( ANDROID_ARCH_FULLNAME "x86_64" )
 set( ANDROID_LLVM_TRIPLE "x86_64-linux-android" )
 set( CMAKE_SYSTEM_PROCESSOR "x86_64" ) 
 set( ANDROID_TOOLCHAIN_MACHINE_NAME "x86_64-linux-android" )
else()
 message( SEND_ERROR "Unknown ANDROID_ABI=\"${ANDROID_ABI}\" is specified." )
endif()

get_filename_component( ANDROID_STANDALONE_TOOLCHAIN "${ANDROID_STANDALONE_TOOLCHAIN}" ABSOLUTE )
set( ANDROID_STANDALONE_TOOLCHAIN "${ANDROID_STANDALONE_TOOLCHAIN}" CACHE INTERNAL "Path of the Android standalone toolchain" FORCE )
set( ANDROID_TOOLCHAIN_ROOT "${ANDROID_STANDALONE_TOOLCHAIN}" CACHE INTERNAL "Android Toolchain Root" FORCE )
set( ANDROID_SYSROOT "${ANDROID_STANDALONE_TOOLCHAIN}/sysroot" CACHE INTERNAL "Android Sysroot" FORCE )

# setup the cross-compiler
if( NOT CMAKE_C_COMPILER )
 set( CMAKE_C_COMPILER   "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-gcc"    CACHE PATH "C compiler" )
 set( CMAKE_CXX_COMPILER "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-g++"    CACHE PATH "C++ compiler" )
 set( CMAKE_ASM_COMPILER "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-gcc"     CACHE PATH "assembler" )
 set( CMAKE_STRIP        "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-strip"   CACHE PATH "strip" )
 set( CMAKE_AR           "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-ar"      CACHE PATH "archive" )
 set( CMAKE_LINKER       "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-ld"      CACHE PATH "linker" )
 set( CMAKE_NM           "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-nm"      CACHE PATH "nm" )
 set( CMAKE_OBJCOPY      "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-objcopy" CACHE PATH "objcopy" )
 set( CMAKE_OBJDUMP      "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-objdump" CACHE PATH "objdump" )
 set( CMAKE_RANLIB       "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_TOOLCHAIN_MACHINE_NAME}-ranlib"  CACHE PATH "ranlib" )
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

# put flags to cache (for debug purpose only)
set( ANDROID_CXX_FLAGS         "${ANDROID_CXX_FLAGS}"         CACHE INTERNAL "Android specific c/c++ flags" )
set( ANDROID_LINKER_FLAGS      "${ANDROID_LINKER_FLAGS}"      CACHE INTERNAL "Android specific c/c++ linker flags" )

# finish flags
set( CMAKE_CXX_FLAGS           "${ANDROID_CXX_FLAGS} ${CMAKE_CXX_FLAGS}" )
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

include_directories( SYSTEM "${ANDROID_SYSROOT}/usr/include" ${ANDROID_TOOLCHAIN_ROOT}/include/python2.7 ${ANDROID_STL_INCLUDE_DIRS} ${ANDROID_ADDITIONAL_INCLUDE_DIRS} )
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