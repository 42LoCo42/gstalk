{
  outputs = { flake-utils, nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        inherit (pkgs.lib.fileset) toSource unions;
        stdenv = pkgs.gccStdenv; # pkgs.clangStdenv;
      in
      rec {
        packages.default = stdenv.mkDerivation {
          pname = "gstalk";
          version = "0.0.1";

          src = toSource {
            root = ./.;
            fileset = unions [
              ./meson.build
              ./src
            ];
          };

          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
            wrapGAppsHook3
          ];

          buildInputs =
            with pkgs;
            with gst_all_1; [
              libpulseaudio

              gstreamer
              gst-plugins-base
              gst-plugins-good
            ];

          mesonBuildType = "release";
          mesonFlags = [ "--werror" ];
        };

        devShells.default = (pkgs.mkShell.override {
          inherit stdenv;
        }) {
          inputsFrom = [ packages.default ];

          packages = with pkgs; [
            clang-tools
            just
            pulseaudio
            qpwgraph
            valgrind
          ];
        };
      });
}
