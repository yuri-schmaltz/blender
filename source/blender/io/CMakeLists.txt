# SPDX-FileCopyrightText: 2020 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later


add_subdirectory(common)

if(WITH_IO_WAVEFRONT_OBJ)
  add_subdirectory(wavefront_obj)
endif()

if(WITH_IO_PLY)
  add_subdirectory(ply)
endif()

if(WITH_IO_SPZ)
endif()

if(WITH_IO_STL)
  add_subdirectory(stl)
endif()

if(WITH_IO_GREASE_PENCIL)
endif()

if(WITH_IO_FBX)
  add_subdirectory(fbx)
endif()

if(WITH_ALEMBIC)
endif()

if(WITH_USD)
endif()

if(WITH_OPENTIMELINEIO)
endif()
