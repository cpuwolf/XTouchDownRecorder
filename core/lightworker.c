/*
LightWorker

BSD 2-Clause License

Copyright (c) 2018, Wei Shuai <cpuwolf@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h> 

#include "lightworker.h"


static void * _lightworker_job_helper(void *arg)
{
    struct lightworker * thread = (struct lightworker *)arg;
    thread->func(thread->priv);
	free(arg);
    return NULL;
}

struct lightworker* lightworker_create(lightworker_job_t func, void *arg)
{
	struct lightworker * thread = malloc(sizeof(struct lightworker));
	if (!thread) return NULL;
    thread->func=func;
    thread->priv=arg;

	lightworker_mutex_init(&thread->mutex);
#if defined(_WIN32)
	thread->thread_id = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE)_lightworker_job_helper, thread, 0, NULL);
	if (!thread->thread_id) {
		free(thread);
		thread = NULL;
	}
#else
	int ret = pthread_create(&thread->thread_id, NULL, _lightworker_job_helper, thread);
	if (ret) {
		free(thread);
		thread = NULL;
	}
#endif
	return thread;
}

void lightworker_destroy(struct lightworker* thread)
{
#if defined(_WIN32)
	CloseHandle(thread->thread_id);
#else
	if (thread->thread_id) {
		pthread_detach(thread->thread_id);
	}
#endif
}


static void lightworker_q_init(lightworker_q * cb, unsigned int max)
{
	cb->g_start = 0;
	cb->g_end = 0;
	cb->g_size = 0;
	cb->g_max_size = max;
	lightworker_mutex_init(&cb->lock);
}

static unsigned int lightworker_q_put_pre(lightworker_q * cb)
{
	return cb->g_end;
}
static BOOL lightworker_q_is_full(lightworker_q * cb)
{
	BOOL ret;
	lightworker_mutex_acquire(&cb->lock);
	ret = (cb->g_size == cb->g_max_size);
	lightworker_mutex_release(&cb->lock);
	return ret;
}
static void lightworker_q_put_post(lightworker_q * cb)
{
	if (cb->g_end < cb->g_max_size) {
		cb->g_end++;
	}
	if (cb->g_end == cb->g_max_size) {
		cb->g_end = 0;
	}
	//cs
	lightworker_mutex_acquire(&cb->lock);
	if (cb->g_size < cb->g_max_size) {
		cb->g_size++;
	}
	lightworker_mutex_release(&cb->lock);
}

static unsigned int lightworker_q_get_pre(lightworker_q * cb)
{
	return cb->g_start;
}
static BOOL lightworker_q_is_empty(lightworker_q * cb)
{
	BOOL ret;
	ret = (cb->g_size == 0);
	return ret;
}
static void lightworker_q_get_post(lightworker_q * cb)
{
	if (cb->g_start < cb->g_max_size) {
		cb->g_start++;
	}
	if (cb->g_start == cb->g_max_size) {
		cb->g_start = 0;
	}
	//cs
	lightworker_mutex_acquire(&cb->lock);
	if (cb->g_size > 0) {
		cb->g_size--;
	}
	lightworker_mutex_release(&cb->lock);
}
//customer queue
#define LIGHTWORKER_QUEUE_TASK_MAX 10

typedef struct {
	lightworker_q q;
	lightworker_queue_task task[LIGHTWORKER_QUEUE_TASK_MAX];
}lightworker_queue;

static lightworker_queue g_wq;

void lightworker_queue_init(lightworker_queue * cb)
{
	lightworker_q_init(&cb->q, LIGHTWORKER_QUEUE_TASK_MAX);
}

void lightworker_queue_put(lightworker_queue * cb, int msg, lightworker_job_t func, void *arg)
{
	unsigned int idx = lightworker_q_put_pre(&cb->q);
	if (!lightworker_q_is_full(&cb->q)) {
		cb->task[idx].msg = msg;
		cb->task[idx].func = func;
		cb->task[idx].priv = arg;
		lightworker_q_put_post(&cb->q);
	}
}

lightworker_queue_task* lightworker_queue_get(lightworker_queue * cb)
{
	unsigned int idx = lightworker_q_get_pre(&cb->q);
	if (!lightworker_q_is_empty(&cb->q)) {
		return cb->task + idx;
		lightworker_q_get_post(&cb->q);
	}
	else {
		//sleep
		return NULL;
	}
}

void lightworker_queue_init_single()
{
	lightworker_queue * cb = &g_wq;
	lightworker_queue_init(cb);
}
void lightworker_queue_put_single(int msg, lightworker_job_t func, void *arg)
{
	lightworker_queue * cb = &g_wq;
	lightworker_queue_put(cb, msg, func, arg);
}
lightworker_queue_task* lightworker_queue_get_single()
{
	lightworker_queue * cb = &g_wq;
	return lightworker_queue_get(cb);
}
