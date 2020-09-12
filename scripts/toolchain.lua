if _ACTION == "clean" then
    os.rmdir(BUILDDIR)
end

location(BUILDDIR)

omitframepointer "on"
symbols "on"

filter "platforms:Linux*"

buildoptions {
    "-Wall",
    "-Wextra",
    "-Wundef",
    "-msse2",
}

linkoptions {
    "-rdynamic",
}

cdialect "gnu11"

links {
    "dl",
}
