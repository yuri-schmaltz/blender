# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

if(WITH_BLENDER)
  if(WITH_INPUT_NDOF)
    add_definitions(-DWITH_INPUT_NDOF)
  endif()

  add_subdirectory(gizmo_library)
  add_subdirectory(id_management)
  add_subdirectory(interface)
  add_subdirectory(io)
  add_subdirectory(mesh)
  add_subdirectory(object)
  add_subdirectory(scene)
  add_subdirectory(space_api)
  add_subdirectory(space_buttons)
  add_subdirectory(space_console)
  add_subdirectory(space_file)
  add_subdirectory(space_info)
  add_subdirectory(space_outliner)
  add_subdirectory(space_statusbar)
  add_subdirectory(space_text)
  add_subdirectory(space_topbar)
  add_subdirectory(space_userpref)
  add_subdirectory(space_view3d)
  add_subdirectory(transform)
  add_subdirectory(undo)
  add_subdirectory(util)
  add_subdirectory(screen)
endif()

add_subdirectory(datafiles)
