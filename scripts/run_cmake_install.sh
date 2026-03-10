#!/bin/bash

current_dir=$(dirname $0)

cmake --install build --prefix ${current_dir}/install -v
