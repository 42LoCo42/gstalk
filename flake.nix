{
  outputs = { flake-utils, nixpkgs, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        inherit (pkgs.lib.fileset) toSource unions;
        stdenv = pkgs.gccStdenv; # pkgs.clangStdenv;
      in
      rec {
        packages.default = stdenv.mkDerivation (drv: {
          pname = "gstalk";
          version = "1.0.0";

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
              pipewire

              gstreamer
              gst-plugins-base
              gst-plugins-good
            ];

          preConfigure = ''
            meson rewrite kwargs set project / version "$version"
          '';

          mesonBuildType = "release";
          mesonFlags = [ "--werror" ];

          doInstallCheck = true;

          nativeInstallCheckInputs = with pkgs; [
            versionCheckHook
          ];

          meta = {
            description = "gstreamer-based media sharing application";
            homepage = "https://github.com/42LoCo42/gstalk";
            mainProgram = drv.pname;
          };
        });

        devShells.default = (pkgs.mkShell.override {
          inherit stdenv;
        }) {
          inputsFrom = [ packages.default ];

          packages = with pkgs; [
            clang-tools
            gdb
            just
            pulseaudio
            qpwgraph
            valgrind
          ];
        };
      });
}
