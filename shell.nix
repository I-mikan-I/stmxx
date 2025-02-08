{
  pkgs ? import <nixpkgs> { },
}:
pkgs.clangStdenv.mkDerivation {
  name = "";
  nativeBuildInputs = with pkgs; [
    cmake
    ninja
    clang-tools_19
    clang_19
    zlib
    openssl.dev
  ];
  "CMAKE_GENERATOR" = "Ninja";
  "CC" = "clang";
  "CXX" = "clang++";
}
