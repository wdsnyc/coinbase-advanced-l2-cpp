#!/bin/bash -x

# CMAKE_EXPORT_COMPILE_COMMANMDS generates compile_commands.json used
# by emacs clangd/lsp mode

VERBOSE=1 cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -B build
