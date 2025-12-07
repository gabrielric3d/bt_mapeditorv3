cmake_minimum_required(VERSION 3.1)

project(rme)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Boost 1.66.0 REQUIRED COMPONENTS date_time system filesystem iostreams)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

find_package(OpenGL REQUIRED)

# LibArchive disabled in compilation level by default, see "#define OTGZ_SUPPORT" in the "definitions.h" file
# If you need use, enable this:
#find_package(LibArchive REQUIRED)
#${LibArchive_INCLUDE_DIRS} ${LibArchive_LIBRARIES}

#if(APPLE)
#    set(CMAKE_PREFIX_PATH /usr/local/opt/libarchive)
#endif()

if(WIN32)
    set(Boost_THREADAPI win32)
endif()

find_package(wxWidgets COMPONENTS html aui gl adv core net base REQUIRED)

find_package(GLUT REQUIRED)
find_package(ZLIB REQUIRED)

include(${wxWidgets_USE_FILE})
include(source/CMakeLists.txt)
add_executable(${PROJECT_NAME} ${rme_H} ${rme_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Deploy artifacts to the project root after every successful build so the
# editor executable and dependent DLLs are ready to ship.
set(RME_RUNTIME_OUTPUT_DIR ${CMAKE_SOURCE_DIR})
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:${PROJECT_NAME}>
		${RME_RUNTIME_OUTPUT_DIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}>
	COMMENT "Copying ${PROJECT_NAME} executable to project root")

set(RME_VCPKG_TRIPLET "${VCPKG_TARGET_TRIPLET}")
if(NOT RME_VCPKG_TRIPLET)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(RME_VCPKG_TRIPLET "x64-windows")
	else()
		set(RME_VCPKG_TRIPLET "x86-windows")
	endif()
endif()

set(RME_VCPKG_BIN_DIR "${CMAKE_SOURCE_DIR}/vcpkg/installed/${RME_VCPKG_TRIPLET}/bin")
if(EXISTS ${RME_VCPKG_BIN_DIR})
	file(GLOB RME_DEPENDENCY_DLLS "${RME_VCPKG_BIN_DIR}/*.dll")
	foreach(dll ${RME_DEPENDENCY_DLLS})
		get_filename_component(dll_name ${dll} NAME)
		add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${dll}"
				"${RME_RUNTIME_OUTPUT_DIR}/${dll_name}"
			COMMENT "Copying ${dll_name} to project root")
	endforeach()
endif()

include_directories(
    ${LibArchive_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${GLUT_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIR}

    )
target_link_libraries(${PROJECT_NAME}
    ${wxWidgets_LIBRARIES}
    ${LibArchive_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
    ${ZLIB_LIBRARIES}
    fmt::fmt
    Boost::date_time
    Boost::system
    Boost::filesystem
    Boost::iostreams
    nlohmann_json::nlohmann_json
)
