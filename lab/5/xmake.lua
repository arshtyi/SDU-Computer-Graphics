set_project("lab5")
set_languages("cxx17")
add_rules("mode.debug", "mode.release")
set_rundir(os.projectdir())
set_warnings("all", "extra")

add_requires("eigen 3", { system = false })
add_requires("opencv 4", { configs = { shared = true, ffmpeg = false, eigen = false } })

target("RenderCore")
set_kind("static")
add_files("lib/*.cpp")
add_includedirs("include", { public = true })
add_sysincludedirs("example/include")
add_packages("eigen", "opencv", { public = true })
target_end()

target("Rasterizer")
set_kind("binary")
add_files("src/*.cpp")
add_deps("RenderCore")
for _, shader in ipairs({ "normal", "phong", "texture", "bump", "bump-phong", "displacement", "displacement-fragment" }) do
    add_tests(shader, { runargs = { "build/test/" .. shader .. ".png", shader, "--size", "128" } })
end
add_tests("help", { runargs = { "--help" } })
add_tests("invalid_shader", { runargs = { "--shader", "unknown" }, should_fail = true })
add_tests("invalid_filter", { runargs = { "--filter", "unknown" }, should_fail = true })
add_tests("invalid_size", { runargs = { "--size", "700x" }, should_fail = true })
add_tests("missing_value", { runargs = { "--model" }, should_fail = true })
add_tests("missing_model", { runargs = { "--model", "missing.obj" }, should_fail = true })
add_tests("missing_texture", { runargs = { "--shader", "texture", "--texture", "missing.png" }, should_fail = true })
add_tests("nonfinite_angle", { runargs = { "--angle", "nan" }, should_fail = true })
add_tests("unknown_option", { runargs = { "--unknown" }, should_fail = true })
target_end()

target("RenderTests")
set_kind("binary")
set_default(false)
add_files("test/src/*.cpp")
add_deps("RenderCore")
for _, name in ipairs({ "projection", "texture", "lighting", "bump", "interpolation", "depth", "clipping", "coverage", "displacement", "models" }) do
    add_tests(name, { runargs = { name } })
end
target_end()
