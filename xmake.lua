set_project("trash")
set_version("0.9.2")

target("trash")
    set_kind("binary")

    if is_plat("windows", "mingw") then
        add_files("src/windows/trash_windows.cpp")
        add_syslinks("ole32", "shell32", "shlwapi", "uuid")

        if is_plat("mingw") then
            add_ldflags("-municode", {force = true})
        end
    elseif is_plat("macosx") then
        add_files("src/macos/trash.m", "src/macos/HGUtils.m", "src/macos/HGCLIUtils.m", "src/macos/fileSize.m")
        add_includedirs("include/macos")
        add_frameworks("AppKit", "ScriptingBridge")
        add_mflags("-Wpartial-availability", "-Wno-unguarded-availability")
        add_mflags("-mmacosx-version-min=10.7", {force = true})
    elseif is_plat("linux") then
        add_files("src/wsl/trash_wsl.cpp")
    end
