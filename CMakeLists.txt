cmake_minimum_required(VERSION 3.21)

project(KillMovePrompt VERSION 0.1.0 LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)
find_path(SkyPromptAPI_INCLUDE_DIRS "SkyPrompt/API.hpp")

add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "you"
    SOURCES
        src/main.cpp
        src/Settings.cpp
        src/KillMovePrompt.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${SkyPromptAPI_INCLUDE_DIRS} src)

# Optional: copy the DLL straight into a mod folder after building.
# set SKYRIM_MODS_FOLDER=C:\MO2\mods   (the DLL goes to "<mods>\KillMovePrompt\SKSE\Plugins")
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUT "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}/SKSE/Plugins")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${OUT}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dist/SKSE/Plugins/KillMovePrompt.ini" "${OUT}"
        VERBATIM)
endif()
