/*
*   obs-pipe-source
*   Copyright (C) 2023 nullsrv
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License along
*   with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>
#include <util/dstr.h>
#include <sys/stat.h>

#include <ecal/ecal.h>
#include <ecal/msg/protobuf/subscriber.h>

#include "frame-manager.h"
#include "image-buffer.h"
#include "graphics-custom.h"
#include "proto/frame.pb.h"


//#define SHOW_TRACE 1

#if defined(_DEBUG) && defined(SHOW_TRACE)
#define LOG_TRACE LOG_INFO
#define TRACE(msg, ...) obs_log(LOG_TRACE, msg, __VA_ARGS__)
#else
#define TRACE(msg, ...) do{}while(0)
#endif

// ========================================================================== //
// Structures
// ========================================================================== //

typedef eCAL::protobuf::CSubscriber<ObsPipe::Proto::Frame> obs_pipe_subscriber_t;
typedef ObsPipe::Proto::Frame obs_pipe_frame_t;

struct pipe_source_t {
    obs_source_t            *source;

    char                    *pipe_name;
    bool                    persistent;
    bool                    linear_alpha;
    
    bool                    loaded;
    int                     last_frame_id;
    int64_t                 last_seen;

    gs_image_buffer_t       image;
    obs_pipe_subscriber_t   subscriber;
    obs_pipe_frame_t        frame;
};

typedef pipe_source_t pipe_source_t;

// ========================================================================== //
// eCAL
// ========================================================================== //
static bool ecal_init(void) {
    int ret_code = eCAL::Initialize(0, NULL, "obs-pipe-subscriber");
    if (ret_code < 0) {
        obs_log(LOG_ERROR, "failed to initialize eCAL (version %s)", eCAL::GetVersionString());
    } else if (ret_code > 1) {
        obs_log(LOG_INFO, "eCAL already initialized (version %s)", eCAL::GetVersionString());
    } else {
        obs_log(LOG_INFO, "initialized eCAL (version %s)", eCAL::GetVersionString());
    }
    
    return ret_code >= 0;
}

static void ecal_finalize(void) {
    int ret_code = eCAL::Finalize();
    if (ret_code < 0) {
        obs_log(LOG_ERROR, "failed to finalize eCAL");
    } else if (ret_code > 1) {
        obs_log(LOG_INFO, "eCAL already finalized");
    } else {
        obs_log(LOG_INFO, "finalized eCAL");
    }
}

// ========================================================================== //
// Pipe Source
// ========================================================================== //
static void pipe_source_load(pipe_source_t *context)
{
    TRACE("pipe_source_load()");

    if (eCAL::Ok()) {
        // Receive frame.
        ObsPipe::Proto::Frame& frame = context->frame;
        bool recv = context->subscriber.Receive(frame);
        if (recv) {
            TRACE("loading frame: %d", frame.id());
            
            // Load image received from subscriber.
            gs_image_buffer_init_from_raw_pixels(
                &context->image,
                (uint8_t *)frame.buffer().data(),
                frame.buffer().size(),
                frame.width(),
                frame.height(),
                GS_BGRA,
                context->linear_alpha
                    ? GS_IMAGE_ALPHA_PREMULTIPLY_SRGB
                    : GS_IMAGE_ALPHA_PREMULTIPLY,
                GS_CS_SRGB
            );
            context->last_frame_id = context->frame.id();
        
            // Init texture.
            obs_enter_graphics();
            gs_image_buffer_init_texture(&context->image);
            obs_leave_graphics();

            if (!context->loaded) {
                obs_log(LOG_WARNING, "failed to load texture");
                context->loaded = false;
            }
        }
    }
}

static void pipe_source_unload(pipe_source_t *context)
{
    TRACE("pipe_source_unload()");

    obs_enter_graphics();
    gs_image_buffer_free(&context->image);
    obs_leave_graphics();
}

static const char *pipe_source_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);

    TRACE("pipe_source_get_name()");
    return obs_module_text("Pipe Source");
}

static uint32_t pipe_source_get_width(void *data)
{
    pipe_source_t *context = (pipe_source_t *)data;

    TRACE("pipe_source_get_width()");
    return context->image.width;
}

static uint32_t pipe_source_get_height(void *data)
{
    pipe_source_t *context = (pipe_source_t *)data;

    TRACE("pipe_source_get_height()");
    return context->image.height;
}

static void pipe_source_get_defaults(obs_data_t *settings)
{
    TRACE("pipe_source_get_defaults()");

    obs_data_set_default_string(settings, "pipe_name", "");
    obs_data_set_default_bool(settings, "unload", false);
    obs_data_set_default_bool(settings, "linear_alpha", false);
}

static obs_properties_t *pipe_source_get_properties(void *data)
{
    pipe_source_t       *context = (pipe_source_t *)data;
    obs_properties_t    *props   = obs_properties_create();

    TRACE("pipe_source_get_properties()");
    
    obs_properties_add_text(props, "pipe_name", obs_module_text("PipeName"), OBS_TEXT_DEFAULT);
    obs_properties_add_bool(props, "unload", obs_module_text("UnloadWhenNotShowing"));
    obs_properties_add_bool(props, "linear_alpha", obs_module_text("LinearAlpha"));
    
    return props;
}

static void pipe_source_update(void *data, obs_data_t *settings)
{
    pipe_source_t *context = (pipe_source_t *)data;
    
    TRACE("pipe_source_update()");

    const char  *pipe_name    = obs_data_get_string(settings, "pipe_name");
    const bool  unload        = obs_data_get_bool  (settings, "unload");
    const bool  linear_alpha  = obs_data_get_bool  (settings, "linear_alpha");

    if (context->pipe_name) {
        bfree(context->pipe_name);
    }
    context->pipe_name      = bstrdup(pipe_name);
    context->persistent     = !unload;
    context->linear_alpha   = linear_alpha;
    context->last_frame_id  = -1;
    context->loaded         = false;
    context->last_seen      = 0;

    obs_log(LOG_INFO, "creating subscriber");
    if (context->subscriber.IsCreated()) {
        context->subscriber.Destroy();
    }
    if (strlen(pipe_name) > 0)
    {
        context->subscriber.Create(pipe_name);
    }
}

static void *pipe_source_create(obs_data_t *settings, obs_source_t *source)
{
    TRACE("pipe_source_create()");

    pipe_source_t *context = new pipe_source_t();

    context->source = source;
    pipe_source_update(context, settings);

    return context;
}

static void pipe_source_destroy(void *data)
{
    pipe_source_t *context = (pipe_source_t *)data;

    TRACE("pipe_source_destroy()");

    context->subscriber.Destroy();

    pipe_source_unload(context);

    if (context->pipe_name) {
        bfree(context->pipe_name);
    }

    delete context;
}

static void pipe_source_activate(void *data)
{
    pipe_source_t *context = (pipe_source_t *)data;

    TRACE("pipe_source_activate()");
}

static void pipe_source_deactivate(void *data)
{
    pipe_source_t *context = (pipe_source_t *)data;

    TRACE("pipe_source_activate()");
}

static void pipe_source_show(void *data)
{
    pipe_source_t *context = (pipe_source_t *)data;
    
    TRACE("pipe_source_show()");

    if (!context->persistent) {
        pipe_source_load(context);
    }
}

static void pipe_source_hide(void *data)
{
    pipe_source_t *context = (pipe_source_t *)data;
    
    TRACE("pipe_source_hide()");

    if (!context->persistent) {
        pipe_source_unload(context);
    }
}

static void pipe_source_tick(void *data, float seconds)
{
    pipe_source_t *context = (pipe_source_t *)data;

    TRACE("pipe_source_tick()");

    if (context->persistent || obs_source_showing(context->source)) {
        pipe_source_load(context);
    } else {
        pipe_source_unload(context);
    }
}

static void pipe_source_render(void *data, gs_effect_t *effect)
{
    pipe_source_t *context = (pipe_source_t *)data;

    TRACE("pipe_source_render()");

    struct gs_image_buffer *const image = &context->image;
    gs_texture_t *const texture = image->texture;
    if (!texture) {
        return;
    }

    const bool previous = gs_framebuffer_srgb_enabled();
    gs_enable_framebuffer_srgb(true);

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

    gs_eparam_t *const param = gs_effect_get_param_by_name(effect, "image");
    gs_effect_set_texture_srgb(param, texture);

    gs_draw_sprite(texture, 0, image->width, image->height);

    gs_blend_state_pop();

    gs_enable_framebuffer_srgb(previous);
}

static enum gs_color_space pipe_source_get_color_space(
    void                        *data,
    size_t                      count,
    const enum gs_color_space   *preferred_spaces
) {
    pipe_source_t      *const context = (pipe_source_t *)data;
    gs_image_buffer_t  *const image   = &context->image;

    UNUSED_PARAMETER(count);
    UNUSED_PARAMETER(preferred_spaces);

    TRACE("pipe_source_get_color_space()");

    return image->texture ? image->color_space : GS_CS_SRGB;
}

static struct obs_source_info pipe_source_info_init()
{
    static struct obs_source_info pipe_source_info = {};

    pipe_source_info.id                     = "pipe_source";
    pipe_source_info.type                   = OBS_SOURCE_TYPE_INPUT;
    pipe_source_info.output_flags           = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
    pipe_source_info.get_name               = pipe_source_get_name;
    pipe_source_info.create                 = pipe_source_create;
    pipe_source_info.destroy                = pipe_source_destroy;
    pipe_source_info.get_width              = pipe_source_get_width;
    pipe_source_info.get_height             = pipe_source_get_height;
    pipe_source_info.get_defaults           = pipe_source_get_defaults;
    pipe_source_info.get_properties         = pipe_source_get_properties;
    pipe_source_info.update                 = pipe_source_update;
    pipe_source_info.activate               = pipe_source_activate;
    pipe_source_info.deactivate             = pipe_source_deactivate;
    pipe_source_info.show                   = pipe_source_show;
    pipe_source_info.hide                   = pipe_source_hide;
    pipe_source_info.video_tick             = pipe_source_tick;
    pipe_source_info.video_render           = pipe_source_render;
    pipe_source_info.icon_type              = OBS_ICON_TYPE_IMAGE;
    pipe_source_info.video_get_color_space  = pipe_source_get_color_space;

    return pipe_source_info;
}

// ========================================================================== //
// Module
// ========================================================================== //
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "OBS Pipe Source";
}

bool obs_module_load(void)
{
    struct obs_source_info pipe_source_info = pipe_source_info_init();

    TRACE("obs_module_load()");

    if (!ecal_init()) {
        return false;
    }

    if (!gs_custom_init_image_deps()) {
        return false;
    }

    obs_register_source(&pipe_source_info);

    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    TRACE("obs_module_unload()");

    ecal_finalize();
    gs_custom_free_image_deps();

    obs_log(LOG_INFO, "plugin unloaded");
}
