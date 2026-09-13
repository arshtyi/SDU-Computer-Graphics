set_project("lab1")
set_languages("cxx17")
add_rules("mode.debug", "mode.release")

package("eigen")
set_kind("library", { headeronly = true })
on_fetch(function()
	import("package.manager.cmake.find_package", { alias = "find_cmake_package" })
	return find_cmake_package("Eigen3", {
		require_version = "3",
		configs = { search_mode = "config", link_libraries = { "Eigen3::Eigen" } },
	})
end)
package_end()
add_requires("eigen", { system = true })

target("PointLocation")
set_kind("binary")
add_files("src/main.cpp")
add_packages("eigen")
set_rundir(".")

for _, name in ipairs({ "basic", "clockwise", "vertex_ray" }) do
	add_tests(name, {
		runargs = { "test/" .. name .. ".in" },
		pass_output_files = "test/" .. name .. ".out",
		plain = true,
		trim_output = true,
	})
end
for _, name in ipairs({ "collinear", "repeated" }) do
	add_tests(name, { runargs = { "test/" .. name .. ".in" }, should_fail = true })
end
target_end()
