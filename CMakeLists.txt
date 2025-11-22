cmake_minimum_required(VERSION 3.15)

if(NOT WIN32)
	set(BUILD_TYPE_INSTRUCTIONS "Choose the type of build, default Release, other options: Debug RelWithDebInfo MinSizeRel.")
	if(CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING ${BUILD_TYPE_INSTRUCTIONS})
	else()
		set(CMAKE_BUILD_TYPE Release CACHE STRING ${BUILD_TYPE_INSTRUCTIONS})
	endif()
endif()

project(MiniPlex LANGUAGES CXX)
add_definitions(-DMP_VERSION="1.3.0-RC2")

file(GLOB ${PROJECT_NAME}_SRC src/*.cpp src/*.h)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) #Eg. -std=c++20 instead of -std=gnu++20

install(FILES "LICENSE" DESTINATION ".")
install(FILES "NOTICE" DESTINATION ".")
install(FILES "README.md" DESTINATION ".")
install(FILES "RELEASE_NOTES" DESTINATION ".")
install(DIRECTORY "./3rdPartyLicenses" DESTINATION ".")
install(DIRECTORY "./Examples" DESTINATION ".")

# Platform configuration
if(WIN32)
	# for ASIO
	add_definitions(-D_WIN32_WINNT=0x0502)	
	add_definitions(-DASIO_HAS_STD_SYSTEM_ERROR)
	add_definitions(-DVC_EXTRALEAN)
	add_definitions(-DWIN32_LEAN_AND_MEAN)
    # Turn off the VS ZERO_CHECK config which runs everytime you build
    set(CMAKE_SUPPRESS_REGENERATION true)
	# UNICODE support
	add_definitions(-DUNICODE -D_UNICODE)

    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP /Zc:rvalueCast")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	endif()
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++")
    #why is this not automatic with Clang?
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        #need to add these to the link libraries manually
        set(LIBCXX "c++" "c++abi")
    endif()
    # different release and debug flags
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
	set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer")
endif()

find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

include(CheckFunctionExists)
check_function_exists(closefrom HAVE_CLOSEFROM)
if(HAVE_CLOSEFROM)
	add_definitions(-DHAVE_CLOSEFROM)
endif()

add_definitions(-DASIO_STANDALONE)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/submodules/asio/.git")
	execute_process(COMMAND git submodule update --init -- src/submodules/asio
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/submodules/tclap/.git")
	execute_process(COMMAND git submodule update --init -- src/submodules/tclap
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/submodules/spdlog/.git")
	execute_process(COMMAND git submodule update --init -- src/submodules/spdlog
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/asio/asio/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/tclap/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/spdlog/include")
target_link_libraries(${PROJECT_NAME} ${LIBCXX})
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")

file(GLOB ProtoConv_SRC src/ProtoConv/*.cpp src/ProtoConv/*.h)
add_executable(ProtoConv ${ProtoConv_SRC})
target_include_directories(ProtoConv PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/asio/asio/include")
target_include_directories(ProtoConv PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/tclap/include")
target_include_directories(ProtoConv PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/spdlog/include")
target_link_libraries(ProtoConv ${LIBCXX})
set_property(TARGET ProtoConv PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
install(TARGETS ProtoConv RUNTIME DESTINATION ".")

