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

#include <apr_general.h>
//#include <unirtsp-sip/sdp.h>

#include "mrcp_unirtsp_server_agent.h"
#include "mrcp_session.h"
#include "mrcp_session_descriptor.h"
#include "rtsp_server.h"
#include "apt_consumer_task.h"
#include "apt_log.h"

typedef struct mrcp_unirtsp_agent_t mrcp_unirtsp_agent_t;
typedef struct mrcp_unirtsp_session_t mrcp_unirtsp_session_t;

struct mrcp_unirtsp_agent_t {
	mrcp_sig_agent_t     *sig_agent;
	rtsp_server_t        *rtsp_server;

	rtsp_server_config_t *config;
};

struct mrcp_unirtsp_session_t {
	mrcp_session_t        *mrcp_session;
	rtsp_server_session_t *rtsp_session;
};


static apt_bool_t server_destroy(apt_task_t *task);
static apt_bool_t server_msg_process(apt_task_t *task, apt_task_msg_t *msg);
static void server_on_start_complete(apt_task_t *task);
static void server_on_terminate_complete(apt_task_t *task);


static apt_bool_t mrcp_unirtsp_on_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor);
static apt_bool_t mrcp_unirtsp_on_session_terminate(mrcp_session_t *session);

static const mrcp_session_response_vtable_t session_response_vtable = {
	mrcp_unirtsp_on_session_answer,
	mrcp_unirtsp_on_session_terminate
};

static apt_bool_t mrcp_unirtsp_event_handler(rtsp_server_t *server, rtsp_server_session_t *session, rtsp_message_t *message);

static apt_bool_t rtsp_config_validate(mrcp_unirtsp_agent_t *agent, rtsp_server_config_t *config, apr_pool_t *pool);


/** Create UniRTSP Signaling Agent */
MRCP_DECLARE(mrcp_sig_agent_t*) mrcp_unirtsp_server_agent_create(rtsp_server_config_t *config, apr_pool_t *pool)
{
	apt_task_vtable_t vtable;
	apt_task_msg_pool_t *msg_pool;
	apt_consumer_task_t *consumer_task;
	mrcp_unirtsp_agent_t *agent;
	agent = apr_palloc(pool,sizeof(mrcp_unirtsp_agent_t));
	agent->sig_agent = mrcp_signaling_agent_create(agent,MRCP_VERSION_1,pool);
	agent->config = config;

	if(rtsp_config_validate(agent,config,pool) == FALSE) {
		return NULL;
	}

	agent->rtsp_server = rtsp_server_create(config->local_ip,config->local_port,config->max_connection_count,pool);
	if(!agent->rtsp_server) {
		return NULL;
	}
	rtsp_server_event_handler_set(agent->rtsp_server,agent,mrcp_unirtsp_event_handler);

	msg_pool = apt_task_msg_pool_create_dynamic(0,pool);

	apt_task_vtable_reset(&vtable);
	vtable.destroy = server_destroy;
	vtable.process_msg = server_msg_process;
	vtable.on_start_complete = server_on_start_complete;
	vtable.on_terminate_complete = server_on_terminate_complete;
	consumer_task = apt_consumer_task_create(agent,&vtable,msg_pool,pool);
	agent->sig_agent->task = apt_consumer_task_base_get(consumer_task);
	apt_log(APT_PRIO_NOTICE,"Create UniRTSP Agent %s:%hu",config->local_ip,config->local_port);
	return agent->sig_agent;
}

/** Allocate UniRTSP config */
MRCP_DECLARE(rtsp_server_config_t*) mrcp_unirtsp_server_config_alloc(apr_pool_t *pool)
{
	rtsp_server_config_t *config = apr_palloc(pool,sizeof(rtsp_server_config_t));
	config->local_ip = NULL;
	config->local_port = 0;
	config->origin = NULL;
	config->resource_location = NULL;
	config->max_connection_count = 100;
	return config;
}

static apt_bool_t rtsp_config_validate(mrcp_unirtsp_agent_t *agent, rtsp_server_config_t *config, apr_pool_t *pool)
{
	agent->config = config;
	return TRUE;
}

static APR_INLINE mrcp_unirtsp_agent_t* server_agent_get(apt_task_t *task)
{
	apt_consumer_task_t *consumer_task = apt_task_object_get(task);
	mrcp_unirtsp_agent_t *agent = apt_consumer_task_object_get(consumer_task);
	return agent;
}

static apt_bool_t server_destroy(apt_task_t *task)
{
	mrcp_unirtsp_agent_t *agent = server_agent_get(task);
	if(agent->rtsp_server) {
		rtsp_server_destroy(agent->rtsp_server);
		agent->rtsp_server = NULL;
	}
	return TRUE;
}

static void server_on_start_complete(apt_task_t *task)
{
	mrcp_unirtsp_agent_t *agent = server_agent_get(task);
	if(agent->rtsp_server) {
		rtsp_server_start(agent->rtsp_server);
	}
}

static void server_on_terminate_complete(apt_task_t *task)
{
	mrcp_unirtsp_agent_t *agent = server_agent_get(task);
	if(agent->rtsp_server) {
		rtsp_server_terminate(agent->rtsp_server);
	}
}

static mrcp_unirtsp_session_t* mrcp_unirtsp_session_create(mrcp_unirtsp_agent_t *agent)
{
	mrcp_unirtsp_session_t *session;
	mrcp_session_t* mrcp_session = agent->sig_agent->create_server_session(agent->sig_agent);
	if(!mrcp_session) {
		return NULL;
	}
	mrcp_session->response_vtable = &session_response_vtable;
	mrcp_session->event_vtable = NULL;

	session = apr_palloc(mrcp_session->pool,sizeof(mrcp_unirtsp_session_t));
	session->mrcp_session = mrcp_session;
	mrcp_session->obj = session;
	
	return session;
}

static apt_bool_t mrcp_unirtsp_event_handler(rtsp_server_t *server, rtsp_server_session_t *rtsp_session, rtsp_message_t *message)
{
	mrcp_unirtsp_session_t *session	= rtsp_server_session_object_get(rtsp_session);
	if(!session) {
		mrcp_unirtsp_agent_t *agent = rtsp_server_object_get(server);
		session = mrcp_unirtsp_session_create(agent);
		if(!session) {
			return FALSE;
		}
		rtsp_server_session_object_set(rtsp_session,session);
	}
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_on_session_answer(mrcp_session_t *session, mrcp_session_descriptor_t *descriptor)
{
	return TRUE;
}

static apt_bool_t mrcp_unirtsp_on_session_terminate(mrcp_session_t *session)
{
	return TRUE;
}

static apt_bool_t server_msg_process(apt_task_t *task, apt_task_msg_t *msg)
{
	return TRUE;
}