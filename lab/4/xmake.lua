set_project("lab4")
set_languages("cxx17")
add_rules("mode.debug", "mode.release")
set_rundir(os.projectdir())
set_warnings("all", "extra")

add_requires("opencv 4", { configs = { shared = true, ffmpeg = false, eigen = false } })

target("Subdivision")
set_kind("static")
add_files("lib/mesh.cpp", "lib/catmull_clark.cpp")
add_includedirs("include", { public = true })
target_end()

target("MeshRender")
set_kind("static")
add_files("lib/render.cpp")
add_deps("Subdivision")
add_includedirs("include", { public = true })
add_packages("opencv", { public = true })
target_end()

target("CatmullClark")
set_kind("binary")
add_files("src/*.cpp")
add_deps("Subdivision", "MeshRender")
for _, name in ipairs({ "required", "warped", "quad", "pentagon", "fan3", "ring" }) do
    add_tests(name, { runargs = { "--input", "test/data/" .. name .. ".obj", "--output", "build/test/" .. name } })
end
add_tests("level0", { runargs = { "--levels", "0", "--output", "build/test/level0" } })
add_tests("help", { runargs = { "--help" } })
for _, value in ipairs({ "-1", "7", "2x" }) do
    add_tests("invalid_levels_" .. value, { runargs = { "--levels", value }, should_fail = true })
end
add_tests("missing_value", { runargs = { "--input" }, should_fail = true })
add_tests("missing_file", { runargs = { "--input", "test/data/missing.obj" }, should_fail = true })
add_tests("unknown_option", { runargs = { "--unknown" }, should_fail = true })
target_end()

target("SubdivisionTests")
set_kind("binary")
set_default(false)
add_files("test/src/*.cpp")
add_deps("Subdivision", "MeshRender")
for _, name in ipairs({ "stencils", "topology", "invariance", "io", "invalid", "render" }) do
    add_tests(name, { runargs = { name, "test/data" } })
end
target_end()
