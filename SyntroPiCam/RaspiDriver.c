/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, James Hughes
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*  RaspiDriver.c (based on RaspiStill.c)
 *
 *  Modifications Copyright (c) 2014, Richard Barnett.
 *
 *  Original license applies - see above.
 *
 *  RaspiStill.c comments follow:
 **/

/**
 * \file RaspiStill.c
 * Command line program to capture a still frame and encode it to file.
 * Also optionally display a preview/viewfinder of current camera input.
 *
 * \date 31 Jan 2013
 * \Author: James Hughes
 *
 * Description
 *
 * 3 components are created; camera, preview and JPG encoder.
 * Camera component has three ports, preview, video and stills.
 * This program connects preview and stills to the preview and jpg
 * encoder. Using mmal we don't need to worry about buffers between these
 * components, but we do need to handle buffers from the encoder, which
 * are simply written straight to the file in the requisite buffer callback.
 *
 * We use the RaspiCamControl code to handle the specific camera settings.
 */

// We use some GNU extensions (asprintf, basename)
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>

#define VERSION_STRING "v1.3.6"

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"


#include "RaspiCamControl.h"
#include "RaspiPreview.h"
//#include "RaspiCLI.h"
//#include "RaspiTex.h"

#include "RaspiDriver.h"

#include <semaphore.h>

/// Camera number to use - we only have one camera, indexed from 0.
#define CAMERA_NUMBER 0

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2


// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

#define MAX_USER_EXIF_TAGS      32
#define MAX_EXIF_PAYLOAD_LENGTH 128

/// Frame advance method
#define FRAME_NEXT_SINGLE        0
#define FRAME_NEXT_TIMELAPSE     1
#define FRAME_NEXT_KEYPRESS      2
#define FRAME_NEXT_FOREVER       3
#define FRAME_NEXT_GPIO          4
#define FRAME_NEXT_SIGNAL        5
#define FRAME_NEXT_IMMEDIATELY   6

int mmal_status_to_int(MMAL_STATUS_T status);

//  This is where the jpeg frame is assembled

#define RASPIDRIVER_MAX_JPEG    300000

static unsigned char jpegBuffer[RASPIDRIVER_MAX_JPEG];
static int jpegLength;

/** Structure containing all state information for the current run
 */
typedef struct
{
    int width;                          /// Requested width of image
    int height;                         /// requested height of image
    int quality;                        /// JPEG quality setting (1-100)
    MMAL_PARAM_THUMBNAIL_CONFIG_T thumbnailConfig;
    int verbose;                        /// !0 if want detailed run information
    MMAL_FOURCC_T encoding;             /// Encoding to use for the output file.
    int fullResPreview;                 /// If set, the camera preview port runs at capture resolution. Reduces fps.

    RASPIPREVIEW_PARAMETERS preview_parameters;    /// Preview setup parameters
    RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
    MMAL_COMPONENT_T *encoder_component;   /// Pointer to the encoder component
    MMAL_COMPONENT_T *null_sink_component; /// Pointer to the null sink component
    MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
    MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection from camera to encoder

    MMAL_POOL_T *encoder_pool; /// Pointer to the pool of buffers used by encoder output port

} RASPIDRIVER_STATE;

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct
{
    FILE *file_handle;                   /// File handle to write buffer data to.
    VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
    RASPIDRIVER_STATE *pstate;            /// pointer to our state in case required in callback
} PORT_USERDATA;


// Our main data storage vessel..
RASPIDRIVER_STATE state;
int exit_code = EX_OK;

MMAL_STATUS_T status = MMAL_SUCCESS;
MMAL_PORT_T *camera_preview_port = NULL;
MMAL_PORT_T *camera_video_port = NULL;
MMAL_PORT_T *camera_still_port = NULL;
MMAL_PORT_T *preview_input_port = NULL;
MMAL_PORT_T *encoder_input_port = NULL;
MMAL_PORT_T *encoder_output_port = NULL;
PORT_USERDATA callback_data;


/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void default_status(RASPIDRIVER_STATE *state)
{
    if (!state) {
        vcos_assert(0);
        return;
    }

    state->width = 640;
    state->height = 360;
    state->quality = 20;
    state->verbose = 0;
    state->thumbnailConfig.enable = 0;
    state->thumbnailConfig.width = 64;
    state->thumbnailConfig.height = 48;
    state->thumbnailConfig.quality = 35;
    state->camera_component = NULL;
    state->encoder_component = NULL;
    state->preview_connection = NULL;
    state->encoder_connection = NULL;
    state->encoder_pool = NULL;
    state->encoding = MMAL_ENCODING_JPEG;
    state->fullResPreview = 0;

    // Setup preview window defaults
    raspipreview_set_defaults(&state->preview_parameters);

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults(&state->camera_parameters);

}


/**
 *  buffer header callback function for camera control
 *
 *  No actions taken in current version
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (buffer->cmd != MMAL_EVENT_PARAMETER_CHANGED)
        vcos_log_error("Received unexpected camera control callback event, 0x%08x %lx",
                     buffer->cmd, (long)port);

    mmal_buffer_header_release(buffer);
}

/**
 *  buffer header callback function for encoder
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    int complete = 0;

    // We pass our file handle and other stuff in via the userdata field.

    PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

    if (pData) {
        if ((buffer->length + jpegLength) > RASPIDRIVER_MAX_JPEG) {
            vcos_log_error("Jpeg too long - discarding chunk");
        } else {
            memcpy(jpegBuffer + jpegLength, buffer->data, buffer->length);
            jpegLength += buffer->length;
        }

        // Now flag if we have completed
        if (buffer->flags &
              (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)) {
            complete = 1;
            if (state.verbose)
                fprintf(stderr, "jpeg size %d\n", jpegLength);
        }
    } else {
        vcos_log_error("Received a encoder buffer callback with no state");
    }

    // release buffer back to the pool
    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled) {
        MMAL_STATUS_T status = MMAL_SUCCESS;
        MMAL_BUFFER_HEADER_T *new_buffer;

        new_buffer = mmal_queue_get(pData->pstate->encoder_pool->queue);

        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);
        if (!new_buffer || status != MMAL_SUCCESS)
            vcos_log_error("Unable to return a buffer to the encoder port");
    }

    if (complete)
        vcos_semaphore_post(&(pData->complete_semaphore));
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct. camera_component member set to the created camera_component if successfull.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
static MMAL_STATUS_T create_camera_component(RASPIDRIVER_STATE *state)
{
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Failed to create camera component");
        goto error;
    }

    if (!camera->output_num) {
        status = MMAL_ENOSYS;
        vcos_log_error("Camera doesn't have output ports");
        goto error;
    }

    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, camera_control_callback);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
        {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = state->width,
            .max_stills_h = state->height,
            .stills_yuv422 = 0,
            .one_shot_stills = 1,
            .max_preview_video_w = state->preview_parameters.previewWindow.width,
            .max_preview_video_h = state->preview_parameters.previewWindow.height,
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
        };

        if (state->fullResPreview) {
            cam_config.max_preview_video_w = state->width;
            cam_config.max_preview_video_h = state->height;
        }

        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    {
        MMAL_PARAMETER_ZEROSHUTTERLAG_T shutter_config =
        {
            { MMAL_PARAMETER_ZERO_SHUTTER_LAG, sizeof(shutter_config) },
            .zero_shutter_lag_mode = 1,
            .concurrent_capture = 1
        };

        mmal_port_parameter_set(camera->control, &shutter_config.hdr);
    }

    raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

    // Now set up the port formats

    format = preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    if (state->fullResPreview) {
        // In this mode we are forcing the preview to be generated from the full capture resolution.
        // This runs at a max of 15fps with the OV5647 sensor.
        format->es->video.width = state->width;
        format->es->video.height = state->height;
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = state->width;
        format->es->video.crop.height = state->height;
        format->es->video.frame_rate.num = FULL_RES_PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = FULL_RES_PREVIEW_FRAME_RATE_DEN;
    } else {
        // use our normal preview mode - probably 1080p30
        format->es->video.width = state->preview_parameters.previewWindow.width;
        format->es->video.height = state->preview_parameters.previewWindow.height;
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = state->preview_parameters.previewWindow.width;
        format->es->video.crop.height = state->preview_parameters.previewWindow.height;
        format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;
    }

    status = mmal_port_format_commit(preview_port);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera viewfinder format couldn't be set");
        goto error;
    }

    // Set the same format on the video  port (which we dont use here)
    mmal_format_full_copy(video_port->format, format);
    status = mmal_port_format_commit(video_port);

    if (status  != MMAL_SUCCESS) {
        vcos_log_error("camera video format couldn't be set");
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    format = still_port->format;

    // Set our stills format on the stills (for encoder) port
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = state->width;
    format->es->video.height = state->height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;


    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    state->camera_component = camera;

    if (state->verbose)
        fprintf(stderr, "Camera component done\n");

    return status;

error:

    if (camera)
        mmal_component_destroy(camera);

    return status;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RASPIDRIVER_STATE *state)
{
    if (state->camera_component) {
        mmal_component_destroy(state->camera_component);
        state->camera_component = NULL;
    }
}

/**
 * Create the encoder component, set up its ports
 *
 * @param state Pointer to state control struct. encoder_component member set to the created camera_component if successfull.
 *
 * @return a MMAL_STATUS, MMAL_SUCCESS if all OK, something else otherwise
 */
static MMAL_STATUS_T create_encoder_component(RASPIDRIVER_STATE *state)
{
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &encoder);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to create JPEG encoder component");
        goto error;
    }

    if (!encoder->input_num || !encoder->output_num) {
        status = MMAL_ENOSYS;
        vcos_log_error("JPEG encoder doesn't have input/output ports");
        goto error;
    }

    encoder_input = encoder->input[0];
    encoder_output = encoder->output[0];

    // We want same format on input and output
    mmal_format_copy(encoder_output->format, encoder_input->format);

    // Specify out output format
    encoder_output->format->encoding = state->encoding;

    encoder_output->buffer_size = encoder_output->buffer_size_recommended;

    if (encoder_output->buffer_size < encoder_output->buffer_size_min)
        encoder_output->buffer_size = encoder_output->buffer_size_min;

    encoder_output->buffer_num = encoder_output->buffer_num_recommended;

    if (encoder_output->buffer_num < encoder_output->buffer_num_min)
        encoder_output->buffer_num = encoder_output->buffer_num_min;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(encoder_output);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to set format on video encoder output port");
        goto error;
    }

    // Set the JPEG quality level
    status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_Q_FACTOR, state->quality);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to set JPEG quality");
        goto error;
    }

    // Set up any required thumbnail
    {
        MMAL_PARAMETER_THUMBNAIL_CONFIG_T param_thumb = {{MMAL_PARAMETER_THUMBNAIL_CONFIGURATION, sizeof(MMAL_PARAMETER_THUMBNAIL_CONFIG_T)}, 0, 0, 0, 0};

        if ( state->thumbnailConfig.enable &&
           state->thumbnailConfig.width > 0 && state->thumbnailConfig.height > 0 ) {
            // Have a valid thumbnail defined
            param_thumb.enable = 1;
            param_thumb.width = state->thumbnailConfig.width;
            param_thumb.height = state->thumbnailConfig.height;
            param_thumb.quality = state->thumbnailConfig.quality;

        } else {
            param_thumb.enable = 0;
            param_thumb.width = 0;
            param_thumb.height = 0;
            param_thumb.quality = 0;
        }
        status = mmal_port_parameter_set(encoder->control, &param_thumb.hdr);
    }

    //  Enable component
    status = mmal_component_enable(encoder);

    if (status  != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable video encoder component");
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

    if (!pool)
      vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);

    state->encoder_pool = pool;
    state->encoder_component = encoder;

    if (state->verbose)
        fprintf(stderr, "Encoder component done\n");

    return status;

error:

    if (encoder)
        mmal_component_destroy(encoder);

    return status;
}

/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_encoder_component(RASPIDRIVER_STATE *state)
{
    // Get rid of any port buffers first
    if (state->encoder_pool)
        mmal_port_pool_destroy(state->encoder_component->output[0], state->encoder_pool);

    if (state->encoder_component) {
        mmal_component_destroy(state->encoder_component);
        state->encoder_component = NULL;
    }
}

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
static MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection)
{
    MMAL_STATUS_T status;

    status =  mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

    if (status == MMAL_SUCCESS) {
        status =  mmal_connection_enable(*connection);
        if (status != MMAL_SUCCESS)
            mmal_connection_destroy(*connection);
    }

    return status;
}


/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
static void check_disable_port(MMAL_PORT_T *port)
{
    if (port && port->is_enabled)
        mmal_port_disable(port);
}


int raspiInit(int width, int height)
{

    bcm_host_init();

    // Register our application with the logging system
    vcos_log_register("RaspiStill", VCOS_LOG_CATEGORY);


    default_status(&state);

    state.width = width;
    state.height = height;

    // OK, we have a nice set of parameters. Now set up our components
    // We have three components. Camera, Preview and encoder.
    // Camera and encoder are different in stills/video, but preview
    // is the same so handed off to a separate module

    if ((status = create_camera_component(&state)) != MMAL_SUCCESS) {
        vcos_log_error("%s: Failed to create camera component", __func__);
        exit_code = EX_SOFTWARE;
        raspicamcontrol_check_configuration(128);
        return exit_code;
    }

    if ((status = raspipreview_create(&state.preview_parameters)) != MMAL_SUCCESS) {
        vcos_log_error("%s: Failed to create preview component", __func__);
        destroy_camera_component(&state);
        exit_code = EX_SOFTWARE;
        raspicamcontrol_check_configuration(128);
        return exit_code;
    }

    if ((status = create_encoder_component(&state)) != MMAL_SUCCESS) {
        vcos_log_error("%s: Failed to create encode component", __func__);
        raspipreview_destroy(&state.preview_parameters);
        destroy_camera_component(&state);
        exit_code = EX_SOFTWARE;
        raspicamcontrol_check_configuration(128);
        return exit_code;
    }

    if (state.verbose)
        fprintf(stderr, "Starting component connection stage\n");

    camera_preview_port = state.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
    camera_video_port   = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
    camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
    encoder_input_port  = state.encoder_component->input[0];
    encoder_output_port = state.encoder_component->output[0];

    if (state.verbose)
         fprintf(stderr, "Connecting camera preview port to video render.\n");

    // Note we are lucky that the preview and null sink components use the same input port
    // so we can simple do this without conditionals
    preview_input_port  = state.preview_parameters.preview_component->input[0];

    // Connect camera to preview (which might be a null_sink if no preview required)
    status = connect_ports(camera_preview_port, preview_input_port, &state.preview_connection);

    if (status == MMAL_SUCCESS) {
        VCOS_STATUS_T vcos_status;

        if (state.verbose)
            fprintf(stderr, "Connecting camera stills port to encoder input port\n");

        // Now connect the camera to the encoder
        status = connect_ports(camera_still_port, encoder_input_port, &state.encoder_connection);

        if (status != MMAL_SUCCESS) {
            vcos_log_error("%s: Failed to connect camera video port to encoder input", __func__);
            goto error;
        }

        // Set up our userdata - this is passed though to the callback where we need the information.
        // Null until we open our filename
        callback_data.file_handle = NULL;
        callback_data.pstate = &state;
        vcos_status = vcos_semaphore_create(&callback_data.complete_semaphore, "RaspiStill-sem", 0);

        vcos_assert(vcos_status == VCOS_SUCCESS);

        if (status != MMAL_SUCCESS) {
            vcos_log_error("Failed to setup encoder output");
            goto error;
        }
    } else {
        mmal_status_to_int(status);
        vcos_log_error("%s: Failed to connect camera to preview", __func__);
    }

    // Enable the encoder output port
    encoder_output_port->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data;

    if (state.verbose)
        fprintf(stderr, "Enabling encoder output port\n");

    // Enable the encoder output port and tell it its callback function
    status = mmal_port_enable(encoder_output_port, encoder_buffer_callback);

    if (mmal_status_to_int(mmal_port_parameter_set_uint32(state.camera_component->control,
                 MMAL_PARAMETER_SHUTTER_SPEED, state.camera_parameters.shutter_speed) != MMAL_SUCCESS))
        vcos_log_error("Unable to set shutter speed");

    int num, q;

    // Send all the buffers to the encoder output port
    num = mmal_queue_length(state.encoder_pool->queue);

    for (q=0;q<num;q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.encoder_pool->queue);

        if (!buffer)
            vcos_log_error("Unable to get a required buffer %d from pool queue", q);
        if (mmal_port_send_buffer(encoder_output_port, buffer)!= MMAL_SUCCESS)
            vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
    }

    return status;

error:

    mmal_status_to_int(status);

    if (state.verbose)
        fprintf(stderr, "Closing down\n");

    raspiClose();

    if (status != MMAL_SUCCESS)
        raspicamcontrol_check_configuration(128);

    return exit_code;
}

void raspiClose()
{
    // Disable encoder output port
    status = mmal_port_disable(encoder_output_port);

    vcos_semaphore_delete(&callback_data.complete_semaphore);

    // Disable all our ports that are not handled by connections
    check_disable_port(camera_video_port);
    check_disable_port(encoder_output_port);

    if (state.preview_connection)
       mmal_connection_destroy(state.preview_connection);

    if (state.encoder_connection)
       mmal_connection_destroy(state.encoder_connection);


    /* Disable components */
    if (state.encoder_component)
       mmal_component_disable(state.encoder_component);

    if (state.preview_parameters.preview_component)
       mmal_component_disable(state.preview_parameters.preview_component);

    if (state.camera_component)
       mmal_component_disable(state.camera_component);

    destroy_encoder_component(&state);
    raspipreview_destroy(&state.preview_parameters);
    destroy_camera_component(&state);

    if (state.verbose)
       fprintf(stderr, "Close down completed, all components disconnected, disabled and destroyed\n\n");
}


int raspiStartCapture()
{
    if (state.verbose)
       fprintf(stderr, "Starting capture\n");

    jpegLength = 0;

    if (mmal_port_parameter_set_boolean(camera_still_port, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS) {
       vcos_log_error("%s: Failed to start capture", __func__);
       return -1;
    }
    return EX_OK;
}

int raspiFinishCapture()
{
    vcos_semaphore_wait(&callback_data.complete_semaphore);
    if (state.verbose)
        fprintf(stderr, "Finished capture\n");

    return EX_OK;
}

void raspiGetJpegBuffer(unsigned char **buffer, int *length)
{
    *length = jpegLength;
    *buffer = jpegBuffer;
}
