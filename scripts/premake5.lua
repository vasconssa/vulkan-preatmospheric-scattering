require "export-compile-commands"

DIR = path.getabsolute("..")
print(DIR)

THIRDPARTYDIR = path.join(DIR, "3rdparty")
BUILDDIR = path.join(DIR, "build")
TESTDIR = path.join(DIR, "tests")

workspace "Gabibits"
    configurations { "Debug", "Release" }
    platforms { "Android-Arm", "Win32", "Win64", "Linux32", "Linux64" }
    toolset "clang"

include("toolchain.lua")

project "sx"
    kind "StaticLib"
    language "C"

    includedirs {
        path.join(DIR, "include"),
        path.join(DIR, "3rdparty"),
    }

    links {
        "m",
        "pthread",
    }

    files {
        path.join(DIR, "src/sx/*.c"),
        --DIR .. "/src/sx/*.c",
    }

    filter "platforms:Linux64"
    system "Linux"
    architecture "x86_64"

    files { 
        DIR .. "/src/sx/asm/make_x86_64_sysv_elf_gas.S",
        DIR .. "/src/sx/asm/jump_x86_64_sysv_elf_gas.S",
        DIR .. "/src/sx/asm/ontop_x86_64_sysv_elf_gas.S",
    }
    filter "architecture:x86"
    system "Linux"
    filter {}

project "volk"
    kind "StaticLib"
    language "C"

    includedirs {
        path.join(DIR, "3rdparty/volk"),
    }



    filter "platforms:Linux64"
    system "Linux"
    architecture "x86_64"
    symbols "on"
    defines {
        "VK_USE_PLATFORM_XCB_KHR",
    }

    links {
        "pthread",
        "m",
    }
    
    filter {}

    files {
        path.join(THIRDPARTYDIR, "volk/volk.c");
    }



project "Vulkan-preatmospheric-scattering"
    kind "WindowedApp"
    language "C"

    includedirs {
        path.join(DIR, "include"),
        path.join(DIR, "3rdparty"),
    }

    links {
        "sx",
        "volk",
    }
    
    prebuildcommands('{MKDIR} %{cfg.buildtarget.directory}/shaders')


    filter "platforms:Linux64"
    system "Linux"
    architecture "x86_64"
    symbols "on"
    defines {
        "GB_USE_XCB",
        "VK_USE_PLATFORM_XCB_KHR",
    }

    links {
        "xcb-util",
        "xcb-ewmh",
        "xcb-keysyms",
        "xcb",
        "pthread",
        "m",
        --"vulkan",
    }
    filter "files:**.comp or **.vert or **.frag or **.glsl"
    -- A message to display while this build step is running (optional)
    buildmessage 'Compiling %{file.relpath}'

    -- One or more commands to run (required)
    buildcommands {
      'glslc "%{file.relpath}" -I'..DIR..'/shaders/preatmospheric -o "%{cfg.buildtarget.directory}/shaders/%{file.name}.spv"',
      'glslc "%{file.relpath}" -I'..DIR..'/shaders/preatmospheric -o'..DIR..'"/shaders/%{file.name}.spv"',
    }

    buildoutputs {
        '%{cfg.buildtarget.directory}/shaders/%{file.name}.spv',
        DIR..'"/shaders/%{file.name}.spv"',
    }
    
    filter {}

    files {
        path.join(DIR, "src/**.c"),
        path.join(DIR, "shaders/*.comp"),
        path.join(DIR, "shaders/*.vert"),
        path.join(DIR, "shaders/*.frag"),
        path.join(DIR, "shaders/sky/*"),
    }

    removefiles {
        path.join(DIR, "src/sx/*.c"),
    }




include "tests.lua"
