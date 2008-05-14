/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MPF_RTP_DESCRIPTOR_H__
#define __MPF_RTP_DESCRIPTOR_H__

/**
 * @file mpf_rtp_descriptor.h
 * @brief MPF RTP Stream Descriptor
 */ 

#include <apr_network_io.h>
#include "mpf_stream_mode.h"
#include "mpf_codec_descriptor.h"

APT_BEGIN_EXTERN_C

/** Enumeration of RTP media descriptor types */
typedef enum {
	RTP_MEDIA_DESCRIPTOR_NONE   = 0x0, /**< no media descriptor */
	RTP_MEDIA_DESCRIPTOR_LOCAL  = 0x1, /**< local media descriptor */
	RTP_MEDIA_DESCRIPTOR_REMOTE = 0x2  /**< remote media descriptor */
} mpf_rtp_media_descriptor_e;

/** RTP media descriptor declaration */
typedef struct mpf_rtp_media_descriptor_t mpf_rtp_media_descriptor_t;
/** RTP stream descriptor declaration */
typedef struct mpf_rtp_stream_descriptor_t mpf_rtp_stream_descriptor_t;
/** RTP termination descriptor declaration */
typedef struct mpf_rtp_termination_descriptor_t mpf_rtp_termination_descriptor_t;


/** RTP media (local/remote) descriptor */
struct mpf_rtp_media_descriptor_t {
	/** IP address */
	apt_str_t        ip;
	/** network port */
	apr_port_t       port;

	/** packetization time */
	apr_uint16_t     ptime;
	/** codec list */
	mpf_codec_list_t codec_list;
};

/** RTP stream descriptor */
struct mpf_rtp_stream_descriptor_t {
	/** Stream mode (send/receive) */
	mpf_stream_mode_e          mode;
	/** Stream mask (local/remote) */
	mpf_rtp_media_descriptor_e mask;
	/** Local media descriptor */
	mpf_rtp_media_descriptor_t local;
	/** Remote media descriptor */
	mpf_rtp_media_descriptor_t remote;
};

/** RTP termination descriptor */
struct mpf_rtp_termination_descriptor_t {
	/** Audio stream descriptor */
	mpf_rtp_stream_descriptor_t audio;
	/** Video stream descriptor */
	mpf_rtp_stream_descriptor_t video;
};


/** Initialize media descriptor */
static APR_INLINE void mpf_rtp_media_descriptor_init(mpf_rtp_media_descriptor_t *media)
{
	apt_string_reset(&media->ip);
	media->port = 0;
	media->ptime = 0;
	mpf_codec_list_reset(&media->codec_list);
}

/** Initialize stream descriptor */
static APR_INLINE void mpf_rtp_stream_descriptor_init(mpf_rtp_stream_descriptor_t *stream)
{
	stream->mode = STREAM_MODE_NONE;
	stream->mask = RTP_MEDIA_DESCRIPTOR_NONE;
	mpf_rtp_media_descriptor_init(&stream->local);
	mpf_rtp_media_descriptor_init(&stream->remote);
}

APT_END_EXTERN_C

#endif /*__MPF_RTP_DESCRIPTOR_H__*/