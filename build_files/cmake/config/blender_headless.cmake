# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# headless configuration, useful in for servers or renderfarms
# builds without a windowing system (X11/Windows/Cocoa).
#
# Example usage:
#   cmake -C../blender/build_files/cmake/config/blender_headless.cmake  ../blender
#

set(WITH_HEADLESS            ON  CACHE BOOL "" FORCE)


# other features which are not especially useful as a python module
set(WITH_X11_XINPUT          OFF CACHE BOOL "" FORCE)
set(WITH_INPUT_NDOF          OFF CACHE BOOL "" FORCE)
