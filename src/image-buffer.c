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

#include "image-buffer.h"
#include "graphics-custom.h"
#include "plugin-support.h"

#include <util/base.h>

static uint64_t calc_mem_usage(gs_image_buffer_t *image)
{
    return image
        ? image->width * image->height * gs_get_format_bpp(image->color_format) / 8
        : 0
        ;
}

static void gs_image_buffer_init_internal(
    gs_image_buffer_t           *image,
    uint8_t                     *buffer,
    size_t                      length,
    enum gs_image_alpha_mode    alpha_mode,
    uint64_t                    *mem_usage,
    bool                        is_raw
) {
    if (!image) {
        return;
    }
    gs_texture_t *saved = image->texture; // HACK: fix this
    memset(image, 0, sizeof(*image));

    if (!buffer) {
        return;
    }
    image->texture = saved; // HACK: fix this


    if (!is_raw) {
        obs_log(LOG_DEBUG, "loading image from buffer");
        image->texture_data = gs_get_pixel_data_from_buffer(
            buffer,
            length,
            alpha_mode,
            &image->color_format,
            &image->width,
            &image->height,
            &image->color_space,
            &image->internal_data_buf,
            &image->internal_data_len
        );
    } else {
        obs_log(LOG_DEBUG, "loading image using raw pixel data");
        image->texture_data      = buffer;
        image->internal_data_buf = NULL;
        image->internal_data_len = 0;
    }

    image->alpha_mode = alpha_mode;

    if (mem_usage) {
        *mem_usage = calc_mem_usage(image);
    }

    image->loaded = !!image->texture_data;
    if (!image->loaded) {
        obs_log(LOG_ERROR, "failed to load image");
        gs_image_buffer_free(image);
    }
}

void gs_image_buffer_init(
    gs_image_buffer_t           *image,
    const uint8_t               *buffer,
    size_t                      length,
    enum gs_image_alpha_mode    alpha_mode
) {
    gs_image_buffer_init_internal(
        image, (uint8_t *)buffer, length, alpha_mode, &image->mem_usage, false
    );
}

void gs_image_buffer_init_raw_pixels(
    gs_image_buffer_t           *image,
    uint8_t                     *buffer,
    size_t                      length,
    uint32_t                    width,
    uint32_t                    height,
    enum gs_color_format        color_format,
    enum gs_image_alpha_mode    alpha_mode,
    enum gs_color_space         color_space
) {
    gs_image_buffer_init_internal(
        image, buffer, length, alpha_mode, NULL, true
    );

    if (image && image->loaded) {
        image->width        = width;
        image->height       = height;
        image->color_format = color_format;
        image->color_space  = color_space;
        image->mem_usage    = calc_mem_usage(image);
    }
}

void gs_image_buffer_free(gs_image_buffer_t *image)
{
    if (!image) {
        return;
    }

    if (image->loaded) {
        gs_texture_destroy(image->texture);
    }

    if (image->internal_data_buf) {
        bfree(image->internal_data_buf);
    }

    memset(image, 0, sizeof(*image));
}

void gs_image_buffer_init_texture(gs_image_buffer_t *image)
{
    if (!image->loaded) {
        return;
    }

    obs_log(LOG_DEBUG, "creating texture");
    image->texture = gs_texture_create(
        image->width,
        image->height,
        image->color_format,
        1,
        (const uint8_t **)&image->texture_data,
        GS_DYNAMIC
    );

    if (!image->texture) {
        obs_log(LOG_ERROR, "failed to create texture");
    }
}

void gs_image_buffer_update_texture(gs_image_buffer_t *image)
{
    if (!image || !image->loaded || !image->texture) {
        return;
    }

    gs_texture_set_image(image->texture, image->texture_data, image->width * 4, false);
}
