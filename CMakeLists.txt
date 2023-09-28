cmake_minimum_required (VERSION 3.24)

project(IpcMutex)

add_library(${PROJECT_NAME} STATIC
	SharedMutex.hpp
	SharedRegion.hpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

target_link_libraries(${PROJECT_NAME} PUBLIC
	rt
	${CMAKE_DL_LIBS}
)

add_executable(${PROJECT_NAME}Test
	test.cpp
)

set_target_properties(${PROJECT_NAME}Test PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${PROJECT_NAME}Test PROPERTY CXX_STANDARD 17)

target_link_libraries(${PROJECT_NAME}Test PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a
	rt
	${CMAKE_DL_LIBS}
)
