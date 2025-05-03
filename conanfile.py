from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy

class IceShardHailstormRecipe(ConanFile):
    name = "hailstorm"
    version = "0.4.0"
    package_type = "static-library"
    user = "iceshard"
    channel = "stable"

    # Optional metadata
    license = "MIT"
    author = "dandielo@iceshard.net"
    url = "https://github.com/iceshard-engine/hailstorm"
    description = "Custom package format for storing resources used by 'iceshard' game engine framework."
    topics = ("resource-packaging", "data-storage", "data", "data-processing", "iceshard")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "LICENSE", "CMakeLists.txt", "private/*", "public/*"

    tool_requires = "cmake/[>=3.25.3 <4.0]", "ninja/[>=1.11.1 <2.0]"

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        self.settings.compiler.cppstd = 20

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, "Ninja")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # Copy the license file
        copy(self, "LICENSE", src=self.folders.source_folder, dst="{}/licenses".format(self.package_folder))

        # Copy CMake installation files
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["hailstorm"]
        self.cpp_info.includedirs = ["public"]
