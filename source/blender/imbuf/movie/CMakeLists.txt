# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
)

set(INC_SYS
)

set(SRC
  MOV_enums.hh
  MOV_read.hh
  MOV_util.hh
  MOV_write.hh

  intern/movie_proxy_indexer.cc
  intern/movie_proxy_indexer.hh
  intern/movie_read.cc
  intern/movie_read.hh
  intern/movie_util.cc
  intern/movie_util.hh
  intern/movie_write.cc
  intern/movie_write.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PUBLIC  bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
)

blender_add_lib(bf_imbuf_movie "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
add_library(bf::imbuf::movie ALIAS bf_imbuf_movie)
