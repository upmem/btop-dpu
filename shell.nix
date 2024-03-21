with import <nixpkgs> {};
pkgs.mkShell {
  buildInputs = [
    clang-tools
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