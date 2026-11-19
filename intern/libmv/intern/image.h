/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_IMAGE_H_
#define LIBMV_IMAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_FloatImage {
  float* buffer;
  int width;
  int height;
  int channels;
} libmv_FloatImage;

void libmv_floatImageDestroy(libmv_FloatImage* image);

void libmv_samplePlanarPatchFloat(const float* image,
                                  int width,
                                  int height,
                                  int channels,
                                  const double* xs,
                                  const double* ys,
                                  int num_samples_x,
                                  int num_samples_y,
                                  const float* mask,
                                  float* patch,
                                  double* warped_position_x,
                                  double* warped_position_y);

void libmv_samplePlanarPatchByte(const unsigned char* image,
                                 int width,
                                 int height,
                                 int channels,
                                 const double* xs,
                                 const double* ys,
                                 int num_samples_x,
                                 int num_samples_y,
                                 const float* mask,
                                 unsigned char* patch,
                                 double* warped_position_x,
                                 double* warped_position_y);

#ifdef __cplusplus
}
#endif

#endif  // LIBMV_IMAGE_H_
