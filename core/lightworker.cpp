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

#if defined(__APPLE__) || defined(__unix__)
#ifndef BOOL
#define BOOL unsigned char
#define TRUE 1
#define FALSE 0
#endif
#endif

#include "lightworker.h"


static void * _lightworker_job_helper(void *arg)
{
    struct lightworker * thread = (struct lightworker *)arg;
    thread->func(thread->priv);
	lightworker_event_set(&thread->event_exit);
	lightworker_event_destroy(&thread->event);
    return NULL;
}

struct lightworker* lightworker_create(lightworker_job_t func, void *arg)
{
	struct lightworker * thread = (struct lightworker *)malloc(sizeof(struct lightworker));
	if (!thread) return NULL;
    thread->func=func;
    thread->priv=arg;

	lightworker_event_init(&thread->event);
	lightworker_event_init(&thread->event_exit);
	lightworker_queue_init_single(thread);
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

void lightworker_wait_exit(struct lightworker* thread)
{
	lightworker_event_set(&thread->event);
	lightworker_event_wait(&thread->event_exit);
}

void lightworker_destroy(struct lightworker* thread)
{
	lightworker_wait_exit(thread);
#if defined(_WIN32)
	CloseHandle(thread->thread_id);
#else
	if (thread->thread_id) {
		pthread_detach(thread->thread_id);
	}
#endif
	free(thread);
}

void lightworker_event_init(lightworker_event * ev)
{
#if defined(_WIN32)
	ev->event = CreateEvent(NULL, FALSE, FALSE, TEXT("lightworker_event"));
#else
	pthread_mutex_init(&ev->mutex, NULL);
	pthread_cond_init(&ev->condition, NULL);
	ev->flag = 0;
#endif
}
int lightworker_event_wait(lightworker_event * ev)
{
#if defined(_WIN32)
	WaitForSingleObject(&ev->event, INFINITE);
#else
	pthread_mutex_lock(&ev->mutex);
    while (!ev->flag)
		pthread_cond_wait(&ev->condition, &ev->mutex);
	ev->flag = 0;
	pthread_mutex_unlock(&ev->mutex);
#endif
	return 0;
}
void lightworker_event_set(lightworker_event * ev)
{
#if defined(_WIN32)
	SetEvent (&ev->event);
#else
	pthread_mutex_lock(&ev->mutex);
	ev->flag = 1;
	pthread_cond_signal(&ev->condition);
	pthread_mutex_unlock(&ev->mutex);
#endif
}

void lightworker_event_destroy(lightworker_event * ev)
{
#if defined(_WIN32)
	CloseHandle(&ev->event);
#else
	pthread_mutex_destroy(&ev->mutex);
	pthread_cond_destroy(&ev->condition);
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
	lightworker_queue_task* task;
	unsigned int idx = lightworker_q_get_pre(&cb->q);
	if (!lightworker_q_is_empty(&cb->q)) {
		task = cb->task + idx;
		lightworker_q_get_post(&cb->q);
		return task;
	}
	else {
		//sleep
		return NULL;
	}
}

void lightworker_queue_init_single(struct lightworker *thread)
{
	lightworker_queue * cb = &thread->wq;
	lightworker_queue_init(cb);
}
void lightworker_queue_put_single(struct lightworker *thread, int msg, lightworker_job_t func, void *arg)
{
	lightworker_queue * cb = &thread->wq;
	lightworker_queue_put(cb, msg, func, arg);
	lightworker_event_set(&thread->event);
}
lightworker_queue_task* lightworker_queue_get_single(struct lightworker *thread)
{
	lightworker_queue * cb = &thread->wq;
	lightworker_event_wait(&thread->event);
	return lightworker_queue_get(cb);
}
void lightworker_sleep(int ms)
{
#if defined(_WIN32)
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}
