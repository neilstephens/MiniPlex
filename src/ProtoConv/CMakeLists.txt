cmake_minimum_required(VERSION 3.15)

project(ProtoConv LANGUAGES CXX)

file(GLOB ${PROJECT_NAME}_SRC *.cpp *.h)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/asio/asio/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/tclap/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/submodules/spdlog/include")
target_link_libraries(${PROJECT_NAME} ${LIBCXX})
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")

