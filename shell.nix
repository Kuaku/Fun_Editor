{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "raylib-dev-shell";
  buildInputs = [
    pkgs.gcc
    pkgs.pkg-config
    pkgs.libGL
    pkgs.SDL2
    pkgs.glibc
    pkgs.raylib 
    pkgs.xorg.libX11
    pkgs.xorg.libXrandr
    pkgs.xorg.libXinerama
    pkgs.xorg.libXcursor
    pkgs.xorg.libXi
    pkgs.xorg.libXext
  ];

  shellHook = ''
    echo "Development shell for raylib project"
    export C_INCLUDE_PATH=${toString ./include}:$C_INCLUDE_PATH
  '';
}

