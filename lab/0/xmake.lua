set_project("lab0")
set_languages("cxx17")
add_rules("mode.debug", "mode.release")

add_requires("eigen 3", { system = false })

target("Example")
set_kind("binary")
add_files("example/main.cpp")
add_packages("eigen")
target_end()

target("Transformation")
set_kind("binary")
add_files("src/main.cpp")
add_packages("eigen")
target_end()
