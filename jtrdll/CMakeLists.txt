CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

PROJECT(jtrdll)

##
## CUDA SDKS
##

FIND_PACKAGE(CUDA REQUIRED)

##
## OPENCL/APP SDK
##

# Auto-select bitness based on platform
IF( NOT BITNESS )
    IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
        SET(BITNESS 64)
		SET(BITNESS_SUFFIX x86_64)
    ELSE()
        SET(BITNESS 32)
		SET(BITNESS_SUFFIX x86)
    ENDIF()
ENDIF()

# Unset OPENCL_LIBRARIES, so that corresponding arch specific libs are found when bitness is changed
UNSET(OPENCL_LIBRARIES CACHE)

# Set platform
IF( NOT UNIX )
	SET(PLATFORM win)
ELSE()
	SET(PLATFORM lnx)
ENDIF()

FIND_PATH( OPENCL_INCLUDE_DIRS 
    NAMES OpenCL/cl.h CL/cl.h
    HINTS $ENV{AMDAPPSDKROOT}/include/
)
MARK_AS_ADVANCED(OPENCL_INCLUDE_DIRS)

FIND_LIBRARY( OPENCL_LIBRARIES
	NAMES OpenCL
	HINTS $ENV{AMDAPPSDKROOT}/lib
	PATH_SUFFIXES ${PLATFORM}${BITNESS} ${BITNESS_SUFFIX}
)
MARK_AS_ADVANCED( OPENCL_LIBRARIES )

IF( OPENCL_INCLUDE_DIRS STREQUAL "" OR OPENCL_LIBRARIES STREQUAL "")
	MESSAGE( FATAL_ERROR "Could not locate OpenCL include & libs" )
ENDIF( )

#MESSAGE(OPENCL_INCLUDE_DIRS: ${OPENCL_INCLUDE_DIRS})
#MESSAGE(OPENCL_LIBRARIES: ${OPENCL_LIBRARIES})

##
## OPENSSL
##

FIND_PACKAGE(OpenSSL REQUIRED)

#MESSAGE("OPENSSL_LIBRARIES: ${OPENSSL_LIBRARIES}")
##
## ZLIB
##

FILE(GLOB zlib_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/zlib/*.h"
)

FILE(GLOB zlib_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/zlib/*.c"
)

SET(ZLIB_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/zlib")


##
## SOURCES
##

FILE(GLOB jtrdll_cuda_HEADERS
    "../src/cuda/*.cuh"
	)

FILE(GLOB jtrdll_cuda_SOURCES
	"../src/cuda/*.cu"
	)	

FILE(GLOB jtrdll_HEADERS
    "../src/*.h"
    "../src/aes/*.h"
    "../src/aes/openssl/*.h"
#    "../src/aes/aesni/*.h"
    "../src/escrypt/*.h"
)

FILE(GLOB jtrdll_SOURCES
    "../src/*.c"
    "../src/*.cpp"
    "../src/aes/*.c"
    "../src/aes/openssl/*.c"
#    "../src/aes/aesni/*.c"
    "../src/escrypt/*.c"
    "*.cpp"
    "*.h"
)

FILE(GLOB FMT_PLUGINS
    "../src/*_fmt_plug.c"
)


##
## Platform
##

SET(PLATFORM_DEFINITIONS "")
SET(PLATFORM_INCLUDE_DIRS "")
SET(PLATFORM_LIBRARIES "")

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

	#STRING(REPLACE "cudart" "cudart_static" CUDA_LIBRARIES ${CUDA_LIBRARIES})
	SET(OPENSSL_LIBRARIES $<$<CONFIG:Debug>:${LIB_EAY_LIBRARY_DEBUG}> $<$<CONFIG:Debug>:${SSL_EAY_LIBRARY_DEBUG}> $<$<NOT:$<CONFIG:Debug>>:${LIB_EAY_LIBRARY_RELEASE}> $<$<NOT:$<CONFIG:Debug>>:${SSL_EAY_LIBRARY_RELEASE}>)

	# Test 32/64 bits
	IF("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	    MESSAGE(STATUS "Target is 64 bits")
	    SET(WINXXBITS Win64)
	ELSE("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	    MESSAGE(STATUS "Target is 32 bits")
	    SET(WINXXBITS Win32)
	ENDIF("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		
	IF(${WINXXBITS} MATCHES "Win64")

		SET(PLATFORM_DEFINITIONS -D_SCL_SECURE_NO_WARNINGS=1 -D_CRT_SECURE_NO_WARNINGS=1 -D_WIN32_WINNT=0x0501)
		SET(PLATFORM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/w64 ${CMAKE_CURRENT_SOURCE_DIR}/windows)
		
		FILE(GLOB w64_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/w64/*.h)
		FILE(GLOB w64_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/w64/*.c)
		FILE(GLOB windows_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.h)
		FILE(GLOB windows_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.c)
		
		LIST(APPEND jtrdll_HEADERS ${w64_HEADERS} ${windows_HEADERS} )
		LIST(APPEND jtrdll_SOURCES ${w64_SOURCES} ${windows_SOURCES} )
		
		SET(X86_64_S "${CMAKE_CURRENT_SOURCE_DIR}/../src/x86-64.S")

		FUNCTION(ASM_MSVC_PRE SOURCE SUFFIX DEFINES OBJECT_OUT)
			FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/w64/as.exe" W64_AS)
			GET_FILENAME_COMPONENT(FILE_NAME ${SOURCE} NAME_WE)
	
			FILE(TO_NATIVE_PATH "${SOURCE}" SOURCE_NATIVE)
			FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}_${SUFFIX}.S_PRE" PRE_FILE)
			FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}_${SUFFIX}.obj" OBJ_FILE)
			FILE(TO_CMAKE_PATH "${OBJ_FILE}" OBJ_FILE_CMAKE)

			SEPARATE_ARGUMENTS( DEFINES WINDOWS_COMMAND ${DEFINES} )
			ADD_CUSTOM_COMMAND( OUTPUT "${OBJ_FILE_CMAKE}"
				COMMAND cl /P "${SOURCE_NATIVE}" /Fi"${PRE_FILE}" /D_WIN64 ${DEFINES} /I${CMAKE_CURRENT_SOURCE_DIR}/w64
				COMMAND ${W64_AS} ${PRE_FILE} -o ${OBJ_FILE}
				DEPENDS ${SOURCE} 
			)
			
			SET(${OBJECT_OUT} "${OBJ_FILE_CMAKE}" PARENT_SCOPE)
		ENDFUNCTION()

		ASM_MSVC_PRE(${X86_64_S} "SSE2" /D__SSE2__=1 ASM_OBJECTS_SSE2)
		ASM_MSVC_PRE(${X86_64_S} "SSSE3" "/D__SSE2__=1 /D__SSSE3__=1" ASM_OBJECTS_SSSE3)
		ASM_MSVC_PRE(${X86_64_S} "SSE41" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1" ASM_OBJECTS_SSE41)
		ASM_MSVC_PRE(${X86_64_S} "AVX" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1 /D__AVX__=1" ASM_OBJECTS_AVX)
		ASM_MSVC_PRE(${X86_64_S} "AVX2" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1 /D__AVX__=1 /D__AVX2__=1" ASM_OBJECTS_AVX2)
		ASM_MSVC_PRE(${X86_64_S} "XOP" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1 /D__XOP__=1" ASM_OBJECTS_XOP)
		
	ENDIF(${WINXXBITS} MATCHES "Win64") 

	IF(${WINXXBITS} MATCHES "Win32")
		SET(PLATFORM_DEFINITIONS -D_SCL_SECURE_NO_WARNINGS=1 -D_CRT_SECURE_NO_WARNINGS=1 -D_WIN32_WINNT=0x0501)
		SET(PLATFORM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/w32 ${CMAKE_CURRENT_SOURCE_DIR}/windows)

		FILE(GLOB w32_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/w32/*.h)
		FILE(GLOB w32_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/w32/*.c)
		FILE(GLOB windows_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.h)
		FILE(GLOB windows_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.c)
		
		LIST(APPEND jtrdll_HEADERS ${w32_HEADERS} ${windows_HEADERS} )
		LIST(APPEND jtrdll_SOURCES ${w32_SOURCES} ${windows_SOURCES} )
		
		SET(X86_SSE_S TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../src/x86-sse.S" )
		SET(X86_S "${CMAKE_CURRENT_SOURCE_DIR}/../src/x86.S" )

		FUNCTION(ASM_MSVC_PRE SOURCE SUFFIX DEFINES OBJECT_OUT)
			FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/w32/yasm-1.3.0-win32.exe" W32_AS)
			GET_FILENAME_COMPONENT(FILE_NAME ${SOURCE} NAME_WE)
	
			FILE(TO_NATIVE_PATH "${SOURCE}" SOURCE_NATIVE)
			FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}_${SUFFIX}.S_PRE" PRE_FILE)
			FILE(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}_${SUFFIX}.obj" OBJ_FILE)
			FILE(TO_CMAKE_PATH "${OBJ_FILE}" OBJ_FILE_CMAKE)

			ADD_CUSTOM_COMMAND( OUTPUT "${OBJ_FILE_CMAKE}"
				COMMAND cl /P "${SOURCE_NATIVE}" /Fi"${PRE_FILE}" ${DEFINES} /I${CMAKE_CURRENT_SOURCE_DIR}/w32
				COMMAND ${W32_AS} ${PRE_FILE} -o ${OBJ_FILE}
				DEPENDS ${SOURCE}
			)

			SET(${OBJECT_OUT} "${OBJ_FILE_CMAKE}" PARENT_SCOPE)
		ENDFUNCTION()

		# ASM_MSVC_PRE("${X86_64_S}" "SSE2" "/D__SSE2__=1" ASM_OBJECTS_SSE2)
		# ASM_MSVC_PRE("${X86_64_S}" "SSSE3" "/D__SSE2__=1 /D__SSSE3__=1" ASM_OBJECTS_SSSE3)
		# ASM_MSVC_PRE("${X86_64_S}" "SSE41" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1" ASM_OBJECTS_SSE41)
		# ASM_MSVC_PRE("${X86_64_S}" "AVX" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1 /D__AVX__=1" ASM_OBJECTS_AVX)
		# ASM_MSVC_PRE("${X86_64_S}" "AVX2" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1 /D__AVX__=1 /D__AVX2__=1" ASM_OBJECTS_AVX2)
		# ADD_MSVC_PRE("${X86_64_S}" "XOP" "/D__SSE2__=1 /D__SSSE3__=1 /D__SSE41__=1 /D__XOP__=1 " ASM_OBJECTS_XOP)
		MESSAGE("32-bit not suppported yet")
		FAIL()

		
	ENDIF(${WINXXBITS} MATCHES "Win32") 

ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin") 

	SET(PLATFORM_DEFINITIONS " -Wignored-attributes -Wdeprecated-declarations ")

	STRING(REPLACE "cudart.dylib" "cudart_static.a" CUDA_LIBRARIES ${CUDA_LIBRARIES})
	
	SET(PLATFORM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/osx /usr/local/include)
	FIND_LIBRARY(GMP_LIBRARY gmp /usr/local/lib)
	SET(PLATFORM_LIBRARIES crypto ${GMP_LIBRARY})
	
	FILE(GLOB osx_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/osx/*.h)
	FILE(GLOB osx_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/osx/*.c)
	
	LIST(APPEND jtrdll_HEADERS ${osx_HEADERS} )
	LIST(APPEND jtrdll_SOURCES ${osx_SOURCES} )
		
	SET ( ASM_OBJECTS "${CMAKE_CURRENT_SOURCE_DIR}/../src/x86-64.S" )
	
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux") 
	SET(PLATFORM_DEFINITIONS " -D__USE_POSIX2=1 -D_GNU_SOURCE=1  -Wformat -fPIC ")

	STRING(REPLACE "cudart.so" "cudart_static.a" CUDA_LIBRARIES ${CUDA_LIBRARIES})
	
	SET(PLATFORM_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/linux /usr/local/include)
	FIND_LIBRARY(GMP_LIBRARY gmp)
	SET(PLATFORM_LIBRARIES ${GMP_LIBRARY})
	
	FILE(GLOB linux_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/linux/*.h)
	FILE(GLOB linux_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/linux/*.c)
	
	LIST(APPEND jtrdll_HEADERS ${linux_HEADERS} )
	LIST(APPEND jtrdll_SOURCES ${linux_SOURCES} )
	
	SET ( ASM_OBJECTS "${CMAKE_CURRENT_SOURCE_DIR}/../src/x86-64.S" )

ENDIF()


##
## JTRDLL
##


# remove non-working formats
LIST(REMOVE_ITEM FMT_PLUGINS ${CMAKE_CURRENT_SOURCE_DIR}/../src/stribog_fmt_plug.c)
LIST(REMOVE_ITEM FMT_PLUGINS ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda_rawsha256_fmt.c)

# remove non-working files
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/KeccakF-1600-reference32BI.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/stribog_fmt_plug.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda_rawsha256_fmt.c)
LIST(REMOVE_ITEM jtrdll_cuda_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda/rawsha256.cu)

# remove #included programs
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/haval_helper.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/sboxes-s.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/sboxes.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/nonstd.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/md_helper.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/mmap-windows.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/uaf_hash.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/rar_common.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/aes/openssl/ossl_aes_crypto.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/escrypt/crypto_scrypt-nosse.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/escrypt/crypto_scrypt-ref.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/escrypt/crypto_scrypt-sse.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/escrypt/scrypt_platform.c)

# remove main() programs
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/best.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/calc_stat.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/cprepair.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/detect.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/genmkvpwd.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/mkvcalcproba.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/para-best.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/raw2dyna.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/SIPdump.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/symlink.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/tgtsnarf.c)

# remove 2john programs
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/dmg2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/luks2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/truecrypt_volume2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/gpg2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/mozilla2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/uaf2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/hccap2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/pfx2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/vncpcap2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/keepass2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/putty2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/wpapcap2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/keychain2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/pwsafe2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/wpapcap2john.h)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/keyring2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/racf2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/zip2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/keystore2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/rar2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/kwallet2john.c)
LIST(REMOVE_ITEM jtrdll_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/ssh2john.c)

# Write out fmt_externs
FILE(WRITE ${CMAKE_BINARY_DIR}/fmt_externs.h "#define FMT_EXTERNS_H 1\n")
FOREACH(FMTPLUG ${FMT_PLUGINS})
	GET_FILENAME_COMPONENT(FMTPLUGNAME ${FMTPLUG} NAME)
	FILE(APPEND ${CMAKE_BINARY_DIR}/fmt_externs.h "#include \"${FMTPLUGNAME}\"\n")
ENDFOREACH(FMTPLUG)
FILE(APPEND ${CMAKE_BINARY_DIR}/fmt_externs.h "#undef FMT_EXTERNS_H\n")

# Write out fmt_registers
FILE(WRITE ${CMAKE_BINARY_DIR}/fmt_registers.h "#define FMT_REGISTERS_H 1\n")
FOREACH(FMTPLUG ${FMT_PLUGINS})
	GET_FILENAME_COMPONENT(FMTPLUGNAME ${FMTPLUG} NAME)
	FILE(APPEND ${CMAKE_BINARY_DIR}/fmt_registers.h "#include \"${FMTPLUGNAME}\"\n")
ENDFOREACH(FMTPLUG)
FILE(APPEND ${CMAKE_BINARY_DIR}/fmt_registers.h "#undef FMT_REGISTERS_H\n")


# Set up includes and definitions

INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR} 
	${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR}/../src/escrypt 
	${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda ${OPENSSL_INCLUDE_DIR} ${PLATFORM_INCLUDE_DIRS} 
	${ZLIB_INCLUDE_DIRS} 
	${CUDA_INCLUDE_DIRS} ${OPENCL_INCLUDE_DIRS})
ADD_DEFINITIONS ( -DAC_BUILT=1 -DHAVE_OPENCL=1 -DHAVE_CUDA=1 -DJTRDLL_EXPORTS=1 -DJTRDLL=1 ${PLATFORM_DEFINITIONS} )

# Manually compile CUDA SHA224/256 code with extra define

#SET_PROPERTY(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda_rawsha256_fmt.c APPEND_STRING PROPERTY COMPILE_FLAGS " -DSHA256=1 ")
#LIST(APPEND jtrdll_SOURCES ${CMAKE_BINARY_DIR}/cuda_rawsha224_fmt.c)
#SET_PROPERTY(SOURCE ${CMAKE_BINARY_DIR}/cuda_rawsha224_fmt.c APPEND_STRING PROPERTY COMPILE_FLAGS " -DSHA224=1 ")

#LIST(REMOVE_ITEM jtrdll_cuda_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda/rawsha256.cu)
#CUDA_COMPILE(RAWSHA256_O ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda/rawsha256.cu OPTIONS -DSHA256=1)
#CUDA_COMPILE(RAWSHA224_O ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda/rawsha256.cu OPTIONS -DSHA224=1)

#SET_SOURCE_FILES_PROPERTIES( ${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda_rawsha256_fmt.c PROPERTIES COMPILE_FLAGS "-DSHA256=1")

# build jtrdll
CUDA_ADD_LIBRARY( jtrdll_cuda STATIC ${jtrdll_cuda_SOURCES} ${jtrdll_cuda_HEADERS} ) #${RAWSHA256_O} ${RAWSHA224_O})
CUDA_ADD_LIBRARY( jtrdll_xop SHARED ${jtrdll_SOURCES} ${jtrdll_HEADERS} ${zlib_SOURCES} ${zlib_HEADERS} ${ASM_OBJECTS_XOP} )
CUDA_ADD_LIBRARY( jtrdll_avx SHARED ${jtrdll_SOURCES} ${jtrdll_HEADERS} ${zlib_SOURCES} ${zlib_HEADERS} ${ASM_OBJECTS_AVX})
CUDA_ADD_LIBRARY( jtrdll_avx2 SHARED ${jtrdll_SOURCES} ${jtrdll_HEADERS} ${zlib_SOURCES} ${zlib_HEADERS} ${ASM_OBJECTS_AVX2} )
CUDA_ADD_LIBRARY( jtrdll_sse41 SHARED ${jtrdll_SOURCES} ${jtrdll_HEADERS} ${zlib_SOURCES} ${zlib_HEADERS} ${ASM_OBJECTS_SSE41} )
CUDA_ADD_LIBRARY( jtrdll_ssse3 SHARED ${jtrdll_SOURCES} ${jtrdll_HEADERS} ${zlib_SOURCES} ${zlib_HEADERS} ${ASM_OBJECTS_SSSE3} )
CUDA_ADD_LIBRARY( jtrdll_sse2 SHARED ${jtrdll_SOURCES} ${jtrdll_HEADERS} ${zlib_SOURCES} ${zlib_HEADERS} ${ASM_OBJECTS_SSE2} )

TARGET_COMPILE_DEFINITIONS(jtrdll_xop PUBLIC __SSE2__=1 __SSSE3__=1 __SSE4_1__=1 __XOP__=1)
TARGET_COMPILE_DEFINITIONS(jtrdll_avx2 PUBLIC __SSE2__=1 __SSSE3__=1 __SSE4_1__=1 __AVX__=1 __AVX2__=1)
TARGET_COMPILE_DEFINITIONS(jtrdll_avx PUBLIC __SSE2__=1 __SSSE3__=1 __SSE4_1__=1 __AVX__=1)
TARGET_COMPILE_DEFINITIONS(jtrdll_sse41 PUBLIC __SSE2__=1 __SSSE3__=1 __SSE4_1__=1 )
TARGET_COMPILE_DEFINITIONS(jtrdll_ssse3 PUBLIC __SSE2__=1 __SSSE3__=1 )
TARGET_COMPILE_DEFINITIONS(jtrdll_sse2 PUBLIC __SSE2__=1)

TARGET_LINK_LIBRARIES( jtrdll_xop jtrdll_cuda ${OPENSSL_LIBRARIES} ${OPENCL_LIBRARIES} ${PLATFORM_LIBRARIES} )
TARGET_LINK_LIBRARIES( jtrdll_avx jtrdll_cuda ${OPENSSL_LIBRARIES} ${OPENCL_LIBRARIES} ${PLATFORM_LIBRARIES} )
TARGET_LINK_LIBRARIES( jtrdll_avx2 jtrdll_cuda ${OPENSSL_LIBRARIES} ${OPENCL_LIBRARIES} ${PLATFORM_LIBRARIES} )
TARGET_LINK_LIBRARIES( jtrdll_sse41 jtrdll_cuda ${OPENSSL_LIBRARIES} ${OPENCL_LIBRARIES} ${PLATFORM_LIBRARIES} )
TARGET_LINK_LIBRARIES( jtrdll_ssse3 jtrdll_cuda ${OPENSSL_LIBRARIES} ${OPENCL_LIBRARIES} ${PLATFORM_LIBRARIES} )
TARGET_LINK_LIBRARIES( jtrdll_sse2 jtrdll_cuda ${OPENSSL_LIBRARIES} ${OPENCL_LIBRARIES} ${PLATFORM_LIBRARIES} )

#ADD_DEPENDENCIES(jtrdll_cuda do_sha_copy)

IF(MSVC)
SET_PROPERTY(SOURCE dllio.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " /EHsc ")
SET_TARGET_PROPERTIES(jtrdll_xop PROPERTIES LINK_FLAGS "/DELAYLOAD:OpenCL.DLL")
SET_TARGET_PROPERTIES(jtrdll_avx PROPERTIES LINK_FLAGS "/DELAYLOAD:OpenCL.DLL")
SET_TARGET_PROPERTIES(jtrdll_avx2 PROPERTIES LINK_FLAGS "/DELAYLOAD:OpenCL.DLL")
SET_TARGET_PROPERTIES(jtrdll_sse41 PROPERTIES LINK_FLAGS "/DELAYLOAD:OpenCL.DLL")
SET_TARGET_PROPERTIES(jtrdll_ssse3 PROPERTIES LINK_FLAGS "/DELAYLOAD:OpenCL.DLL")
SET_TARGET_PROPERTIES(jtrdll_sse2 PROPERTIES LINK_FLAGS "/DELAYLOAD:OpenCL.DLL")
ENDIF()

FOREACH(source ${jtrdll_SOURCES})
	IF(NOT ${source} MATCHES ".*dllio.cpp$" AND NOT ${source} MATCHES ".*.cu$")
		IF(MSVC)
			SET_PROPERTY(SOURCE ${source} APPEND_STRING PROPERTY COMPILE_FLAGS " /FI\"dllio.h\" ")
		ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
			SET_PROPERTY(SOURCE ${source} APPEND_STRING PROPERTY COMPILE_FLAGS " -include \"dllio.h\" ")
		ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
			SET_PROPERTY(SOURCE ${source} APPEND_STRING PROPERTY COMPILE_FLAGS " -include \"dllio.h\" ")
		ENDIF()
	ENDIF()
ENDFOREACH()


# build jtrexe
ADD_EXECUTABLE( jtrexe ${CMAKE_CURRENT_SOURCE_DIR}/jtrexe/jtrexe.cpp ${CMAKE_CURRENT_SOURCE_DIR}/jtrexe/cpuinformation.cpp ${CMAKE_CURRENT_SOURCE_DIR}/jtrexe/cpuinformation.h)
SET_PROPERTY(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/jtrexe/jtrexe.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " /EHsc ")
SET_PROPERTY(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/jtrexe/cpuinformation.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " /EHsc ")


