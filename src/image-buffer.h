/******************************************************************************
    Copyright (C) 2016 by Hugh Bailey <obs.jim@gmail.com>
    Copyright (C) 2023 by nullsrv

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <graphics/graphics.h>
#include <util/base.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gs_image_buffer {
    gs_texture_t                *texture;
    uint8_t                     *texture_data;
    uint32_t                    width;
    uint32_t                    height;
    enum gs_color_format        color_format;
    enum gs_image_alpha_mode    alpha_mode;
    enum gs_color_space         color_space;
    bool                        loaded;
    uint8_t                     *internal_data_buf;
    size_t                      internal_data_len;
    uint64_t                    mem_usage;
};

typedef struct gs_image_buffer gs_image_buffer_t;

void gs_image_buffer_init(
    gs_image_buffer_t           *image,
    const uint8_t               *buffer,
    size_t                      length,
    enum gs_image_alpha_mode    alpha_mode
);

void gs_image_buffer_init_raw_pixels(
    gs_image_buffer_t           *image,
    uint8_t                     *buffer,
    size_t                      length,
    uint32_t                    width,
    uint32_t                    height,
    enum gs_color_format        color_format,
    enum gs_image_alpha_mode    alpha_mode,
    enum gs_color_space         color_space
);

void gs_image_buffer_free(gs_image_buffer_t *image);

void gs_image_buffer_init_texture(gs_image_buffer_t *image);
void gs_image_buffer_update_texture(gs_image_buffer_t *image);

#ifdef __cplusplus
}
#endif
