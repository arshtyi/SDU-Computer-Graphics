set_project("lab3")
set_languages("cxx17")
add_rules("mode.debug", "mode.release")
set_rundir(os.projectdir())

add_requires("opencv 4", { configs = { shared = true, ffmpeg = false, eigen = false } })

target("Bezier")
set_kind("static")
add_files("lib/*.cpp")
add_includedirs("include", { public = true })
add_packages("opencv", { public = true })
set_warnings("all", "extra")
target_end()

target("BezierCurve")
set_kind("binary")
add_files("src/*.cpp")
add_deps("Bezier")
set_warnings("all", "extra")
for _, name in ipairs({ "cubic", "s_curve", "six_points", "boundary", "repeated", "line", "single" }) do
	add_tests(name, {
		runargs = {
			"--input",
			"test/data/" .. name .. ".in",
			"--algorithm",
			"both",
			"--output",
			"build/test/" .. name .. ".png",
		},
	})
end
for _, name in ipairs({ "empty", "invalid_count", "too_many", "incomplete", "outside", "nonfinite", "trailing" }) do
	add_tests(name, { runargs = { "--input", "test/data/" .. name .. ".in" }, should_fail = true })
end
add_tests("invalid_algorithm", { runargs = { "--algorithm", "unknown" }, should_fail = true })
add_tests("invalid_cli_count", { runargs = { "--count", "4x" }, should_fail = true })
add_tests("missing_value", { runargs = { "--input" }, should_fail = true })
add_tests("missing_file", { runargs = { "--input", "test/data/does-not-exist.in" }, should_fail = true })
add_tests("conflicting_count", { runargs = { "--input", "test/data/cubic.in", "--count", "4" }, should_fail = true })
target_end()

target("BezierTests")
set_kind("binary")
set_default(false)
add_files("test/src/*.cpp")
add_deps("Bezier")
set_warnings("all", "extra")
for _, name in ipairs({ "evaluation", "rasterization", "input" }) do
	add_tests(name, { runargs = { name, "test/data" } })
end
target_end()
