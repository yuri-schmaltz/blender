# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(atomic)
add_subdirectory(clog)
add_subdirectory(ghost)
add_subdirectory(guardedalloc)
add_subdirectory(memutil)
add_subdirectory(opensubdiv)
add_subdirectory(profile)
add_subdirectory(mikktspace)
add_subdirectory(eigen)


if(WITH_MOD_REMESH)
  add_subdirectory(dualcon)
endif()

if(WITH_IK_SOLVER)
endif()

if(WITH_IK_ITASC)
endif()

if(WITH_CYCLES)
endif()

if(WITH_BULLET)
endif()


if(WIN32)
  # Only windows needs utf16 converter.
  add_subdirectory(utfconv)

  # Only used for Windows for now.
  add_subdirectory(uriconvert)
endif()

if(WITH_MOD_FLUID)
endif()

if(WITH_UV_SLIM)
endif()

if(WITH_QUADRIFLOW)
  add_subdirectory(quadriflow)
endif()

if(UNIX AND NOT APPLE)
  add_subdirectory(libc_compat)
endif()

if(WITH_RENDERDOC)
endif()

if(UNIX AND NOT APPLE)
  # Important this comes after "ghost" as it uses includes defined by GHOST's CMake.
  if(WITH_GHOST_WAYLAND AND WITH_GHOST_WAYLAND_DYNLOAD)
    add_subdirectory(wayland_dynload)
  endif()
  if(WITH_GHOST_DBUS AND WITH_GHOST_DBUS_DYNLOAD)
    add_subdirectory(dbus_dynload)
  endif()
endif()

if(WITH_DRACO)
  add_subdirectory(draco_bridge)
endif()

if(WITH_MESHOPTIMIZER)
  add_subdirectory(meshoptimizer_bridge)
endif()

if(WITH_KTX)
endif()
