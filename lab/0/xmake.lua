set_project("lab0")

local cxx_standard = "17"
local eigen_version = "3"
local executables = {
	{ name = "Transformation", files = { "src/main.cpp" } },
	{ name = "Example", files = { "example/main.cpp" } },
}

set_languages("cxx" .. cxx_standard)
add_rules("mode.debug", "mode.release")

package("eigen")
set_kind("library", { headeronly = true })
on_fetch(function()
	import("package.manager.cmake.find_package", { alias = "find_cmake_package" })
	return find_cmake_package("Eigen3", {
		require_version = eigen_version,
		configs = { search_mode = "config", link_libraries = { "Eigen3::Eigen" } },
	})
end)
package_end()
add_requires("eigen", { system = true })

for _, executable in ipairs(executables) do
	target(executable.name)
	set_kind("binary")
	add_files(executable.files)
	add_packages("eigen")
	target_end()
end
