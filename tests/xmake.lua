set_xmakever("2.7.2")

set_languages("cxx20")
set_arch("x64")

add_rules("mode.debug","mode.releasedbg", "mode.release")

add_cxflags("/bigobj", "/MP")
add_defines("UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS")

add_requireconfs("*", { debug = is_mode("debug"), lto = not is_mode("debug"), configs = { shared = false } })
add_requires("spdlog 1.10.0", "gtest")

target("tests")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "WINVER=0x0601", "SPDLOG_WCHAR_TO_UTF8_SUPPORT", "SPDLOG_WCHAR_FILENAMES", "SPDLOG_WCHAR_SUPPORT")
    set_filename("Cyberpunk2077.exe")
    set_kind("binary")
    add_files("**.cpp")
    add_includedirs("../")
    add_packages("spdlog", "gtest")