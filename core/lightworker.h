#ifndef LIGHTWORKER_H
#define LIGHTWORKER_H
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


#if defined(_WIN32)
#include <windows.h>
#define lightworker_mutex_t           CRITICAL_SECTION
#define lightworker_thread_t          HANDLE
#define lightworker_mutex_init(m)	  InitializeCriticalSection(m)
#define lightworker_mutex_acquire(m)  EnterCriticalSection(m)
#define lightworker_mutex_release(m)  LeaveCriticalSection(m)
#define lightworker_mutex_destroy(m)  DeleteCriticalSection(m)
#else
#include <pthread.h>
#define lightworker_mutex_t           pthread_mutex_t
#define lightworker_thread_t          pthread_t
#define lightworker_mutex_init(m)     pthread_mutex_init(m, NULL)
#define lightworker_mutex_acquire(m)  pthread_mutex_lock(m)
#define lightworker_mutex_release(m)  pthread_mutex_unlock(m)
#define lightworker_mutex_destroy(m)  pthread_mutex_destroy(m)
#endif

typedef unsigned int(*lightworker_job_t)(void *);

struct lightworker
{
	lightworker_mutex_t mutex;
	lightworker_thread_t thread_id;
	lightworker_job_t func;
    void * priv;
};

struct lightworker* lightworker_create(lightworker_job_t func, void *arg);

#endif //LIGHTWORKER_H