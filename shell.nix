let
  pkgs = import <nixpkgs> {};
in
  (pkgs.buildFHSUserEnv {
    name = "wasi-sdk";
    targetPkgs = p:
      with p; [
        zlib
        ncurses
        libxml2
        wasmtime
        emscripten
        nodejs-16_x
      ];
    runScript = "env EM_CACHE=$HOME/.emscripten_cache bash";
  })
  .env
