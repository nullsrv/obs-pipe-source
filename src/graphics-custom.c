#include "graphics-custom.h"
#include "plugin-support.h"

#include <util/base.h>
#include <util/bmem.h>

//#define MAGICKCORE_QUANTUM_DEPTH    16
//#define MAGICKCORE_HDRI_ENABLE      0

#include <MagickCore/MagickCore.h>

bool gs_custom_init_image_deps(void)
{
    MagickCoreGenesis(NULL, MagickTrue);
    obs_log(LOG_INFO, "initialized MagickCore (version: %s)", GetMagickVersion(NULL));
    return true;
}

void gs_custom_free_image_deps(void)
{
    MagickCoreTerminus();
    obs_log(LOG_INFO, "deinitialized MagickCore");
}

// TODO: It would be best to use ffmpeg as obs.
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
) {
    uint8_t         *data       = NULL;
    ImageInfo       *info       = NULL;
    ExceptionInfo   *exception  = NULL;
    Image           *image      = NULL;

    if (!buffer || length == 0) {
        return NULL;
    }

    if (!pixel_data || !pixel_data_length) {
        return NULL;
    }

    info = CloneImageInfo(NULL);
    exception = AcquireExceptionInfo();

    SetImageInfoBlob(info, buffer, length);
    image = ReadImage(info, exception);
    if (image) {
        size_t local_cx     = image->magick_columns;
        size_t local_cy     = image->magick_rows;
        size_t required_len = local_cx * local_cy * 4;

        // Use existing pixel data if available.
        if (*pixel_data != NULL) {
             if (*pixel_data_length >= required_len) {
                 data = *pixel_data;
             } else {
                 bfree(*pixel_data);
                 data = bmalloc(required_len);
             }
        } else {
            data = bmalloc(required_len);
        }

        ExportImagePixels(image, 0, 0, local_cx, local_cy, "BGRA", CharPixel, data, exception);
        if (exception->severity != UndefinedException) {
            obs_log(
                LOG_WARNING,
                "magickcore warning/error getting pixel data from buffer : %s",
                exception->reason
            );
            bfree(data);
            data = NULL;
        }

        *pixel_data        = data;
        *pixel_data_length = required_len;

        *color_format = GS_BGRA;
        *cx     = (uint32_t)local_cx;
        *cy     = (uint32_t)local_cy;
        *color_space  = GS_CS_SRGB;
        DestroyImage(image);

    } else if (exception->severity != UndefinedException) {
        obs_log(
            LOG_WARNING,
            "magickcore warning/error reading buffer : %s",
            exception->reason
        );
    }

    DestroyImageInfo(info);
    DestroyExceptionInfo(exception);

    return data;
}
