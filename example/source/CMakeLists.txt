#
# Set up the source files and output library
#
set(CURRENT_PROJECT main_output_exe)
set(CURRENT_PROJECT_FRIENDLY_NAME "BindfltAPIDemo")
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

file(
	GLOB HEADER_FILES
	LIST_DIRECTORIES FALSE
	CONFIGURE_DEPENDS
	"${SOURCE_DIR}/*.h"
)

file(
	GLOB SOURCE_FILES
	LIST_DIRECTORIES FALSE
	CONFIGURE_DEPENDS
	"${SOURCE_DIR}/*.cpp"
)

source_group(
	TREE "${SOURCE_DIR}/.."
	FILES
		${HEADER_FILES}
		${SOURCE_FILES}
)

add_executable(
	${CURRENT_PROJECT}
	${SOURCE_FILES}
)

target_include_directories(
	${CURRENT_PROJECT}
	PRIVATE
		"${SOURCE_DIR}"
)

set_target_properties(
	${CURRENT_PROJECT}
	PROPERTIES
		OUTPUT_NAME ${CURRENT_PROJECT_FRIENDLY_NAME}
		MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase"
)

#
# Compiler-specific options
#
target_compile_features(
	${CURRENT_PROJECT}
	PRIVATE
		cxx_std_23
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(
		${CURRENT_PROJECT}
		PRIVATE
			"/utf-8"
			"/sdl"
			"/permissive-"
			"/Zc:preprocessor"
			"/Zc:inline"
			"/EHsc"

			"/W4"
			"/wd4100"	# '': unreferenced formal parameter
			"/wd4200"	# nonstandard extension used: zero-sized array in struct/union
			"/wd4201"	# nonstandard extension used: nameless struct/union
			"/wd4324"	# '': structure was padded due to alignment specifier
	)

	target_link_options(
		${CURRENT_PROJECT}
		PRIVATE
			# Force generate release PDBs: https://learn.microsoft.com/en-us/cpp/build/reference/debug-generate-debug-info
			"$<$<CONFIG:RELEASE>:/DEBUG:FULL;/OPT:REF;/OPT:ICF>"
			# Anonymize RSDS PDB paths
			"$<$<CONFIG:RELEASE>:/PDBALTPATH:$<TARGET_FILE_PREFIX:${CURRENT_PROJECT}>$<TARGET_FILE_BASE_NAME:${CURRENT_PROJECT}>.pdb>"
	)
endif()

target_compile_definitions(
	${CURRENT_PROJECT}
	PRIVATE
		NOMINMAX
		VC_EXTRALEAN
		WIN32_LEAN_AND_MEAN
		_CRT_SECURE_NO_WARNINGS
)
