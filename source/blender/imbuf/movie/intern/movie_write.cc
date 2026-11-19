/* SPDX-FileCopyrightText: 2006 Peter Schlaile.
 * SPDX-FileCopyrightText: 2023-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "movie_write.hh"

#include "BLI_string_ref.hh"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MOV_write.hh"

#include "BKE_report.hh"
#include "BKE_scene.hh"


namespace blender {


MovieWriter *MOV_write_begin(const Scene *scene,
                             const bke::BlenderProject *project,
                             const RenderData *rd,
                             const ImageFormatData *imf,
                             int rectx,
                             int recty,
                             ReportList *reports,
                             bool preview,
                             const char *suffix)
{
  if (imf->imtype != R_IMF_IMTYPE_FFMPEG) {
    BKE_report(reports, RPT_ERROR, "Image format is not a movie format");
    return nullptr;
  }

  MovieWriter *writer = nullptr;
  UNUSED_VARS(scene, project, rd, imf, rectx, recty, reports, preview, suffix);
  return writer;
}

bool MOV_write_append(MovieWriter *writer,
                      const Scene *scene,
                      const bke::BlenderProject *project,
                      const RenderData *rd,
                      const ImageFormatData *imf,
                      int start_frame,
                      int frame,
                      const ImBuf *image,
                      const char *suffix,
                      ReportList *reports)
{
  if (writer == nullptr) {
    return false;
  }

  UNUSED_VARS(scene, project, rd, imf, start_frame, frame, image, suffix, reports);
  return false;
}

void MOV_write_end(MovieWriter *writer)
{
  UNUSED_VARS(writer);
}

void MOV_filepath_from_settings(char filepath[/*FILE_MAX*/ 1024],
                                const Scene *scene,
                                const bke::BlenderProject *project,
                                const RenderData *rd,
                                bool preview,
                                const char *suffix,
                                ReportList *reports)
{
  UNUSED_VARS(scene, project, rd, preview, suffix, reports);
  filepath[0] = '\0';
}

}  // namespace blender
