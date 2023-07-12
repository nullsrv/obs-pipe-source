#pragma once

#include <graphics/graphics.h>
#include <util/base.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gs_custom_init_image_deps(void);
void gs_custom_free_image_deps(void);

uint8_t *gs_get_pixel_data_from_buffer(
    const uint8_t               *buffer,
    size_t                      length,
    enum gs_image_alpha_mode    alpha_mode,
    enum gs_color_format        *color_format,
    uint32_t                    *cx,
    uint32_t                    *cy,
    enum gs_color_space         *color_space,
    uint8_t                     **pixel_data,
    size_t                      *pixel_data_length
);

#ifdef __cplusplus
}
#endif
