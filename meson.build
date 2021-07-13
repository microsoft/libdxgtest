# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

project(
  'libdxgtest',
  'cpp',
  default_options: 'cpp_args=-std=c++17  -Wno-unknown-pragmas -D_LINUX_')

cpp = meson.get_compiler('cpp')

dep_thread = dependency('threads')

dep_libdxg = dependency(
  'libdxg',
  fallback : ['libdxg', 'dep_libdxg'])

dep_dxheaders = dependency(
  'DirectX-Headers',
  fallback : ['DirectX-Headers', 'dep_dxheaders'])

libdxgtest = executable(
  'libdxgtest',
  'src/main.cpp',
  dependencies: [
    dep_dxheaders,
    dep_libdxg,
    dep_thread])
