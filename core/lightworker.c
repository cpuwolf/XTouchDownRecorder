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