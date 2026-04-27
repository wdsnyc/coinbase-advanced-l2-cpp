#!/bin/bash

current_dir=$(pwd)

cmake --install build --prefix ${current_dir}/install -v
