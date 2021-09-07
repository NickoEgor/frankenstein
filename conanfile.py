from conans import ConanFile, CMake, tools
from conans.errors import ConanException

class FrankensteinConan(ConanFile):
    name = "frankenstein"
    version = "0.1"

    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"

    def configure(self):
        if self.settings.compiler == "gcc"\
           and self.settings.compiler.libcxx != "libstdc++11":
            raise ConanException("libstdc++11 required for build, yours is %s" %
                                 self.settings.compiler.libcxx.value)

        self.options["qt"].commercial = False

        self.options["qt"].opengl = "no"
        self.options["qt"].openssl = True

        self.options["qt"].with_pcre2 = False
        self.options["qt"].with_glib = False
        self.options["qt"].with_freetype = False
        self.options["qt"].with_fontconfig = False
        self.options["qt"].with_icu = False
        self.options["qt"].with_harfbuzz = False
        self.options["qt"].with_libjpeg = False
        self.options["qt"].with_libpng = False
        self.options["qt"].with_sqlite3 = False
        self.options["qt"].with_mysql = False
        self.options["qt"].with_pq = False
        self.options["qt"].with_odbc = False
        self.options["qt"].with_sdl2 = False
        self.options["qt"].with_libalsa = False
        self.options["qt"].with_openal = False

        self.options["qt"].GUI = False
        self.options["qt"].widgets = False

        self.options["qt"].qtwebsockets = True

    def requirements(self):
        # TODO: update version from conan with conan repos
        self.requires('protobuf/3.12.4')
        self.requires('grpc/1.34.1@inexorgame/stable')
        self.requires('qt/5.13.2@bincrafters/stable')
        self.requires('openssl/1.1.1h')

    def _configure_cmake(self):
        cmake = CMake(self)
        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        self.output.info("-----package------")
        cmake = self._configure_cmake()
        cmake.install()
        for path in self.deps_cpp_info["qt"].lib_paths:
          self.copy(pattern="*.so*", src=path, dst="lib", keep_path=False)
        self.output.info("-----package-done------")

    def imports(self):
        self.output.info("-----imports------")
        for path in self.deps_cpp_info["qt"].lib_paths:
          self.copy(pattern="*.so*", src=path, dst="lib", keep_path=False)
        self.output.info("-----imports-done------")
