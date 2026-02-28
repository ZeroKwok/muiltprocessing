from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout

class MarkdownEditorConan(ConanFile):
    name = "muiltprocessing"
    version = "0.1.2"
    settings = "os", "compiler", "build_type", "arch"
    
    def requirements(self):
        self.requires("cppzmq/4.11.0")
        self.requires("boost/1.79.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BOOST_ALL_NO_LIB"] = "ON"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()
