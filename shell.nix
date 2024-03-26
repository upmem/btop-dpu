with import <nixpkgs> {};
gcc13Stdenv.mkDerivation {
  name = "btop-gcc13-env";
  buildInputs = [
    clang-tools_17
    llvmPackages_17.libcxx
    bear
    cmake
    nil
    gnumake
    ninja
  ];
  # nativeBuildInputs is usually what you want -- tools you need to run
  # executables
  nativeBuildInputs = [
    gcc
  ];
}
