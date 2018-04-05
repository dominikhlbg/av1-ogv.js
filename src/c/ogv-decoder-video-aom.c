#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <pthread.h>
#include <stdio.h>
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#define AOM_CODEC_DISABLE_COMPAT 1
#include <aom/aom_decoder.h>
#include <aom/aomdx.h>

#include "ogv-decoder-video.h"

static aom_codec_ctx_t    aomContext;
static aom_codec_iface_t *aomDecoder;

#ifdef __EMSCRIPTEN_PTHREADS__

static double cpu_time = 0.0;
static int busy = 0;

static pthread_t decode_thread;
static pthread_mutex_t decode_mutex; // to hold critical section
static pthread_cond_t ping_cond; // to trigger on new input

typedef struct {
	const char *data;
	size_t data_len;
} decode_queue_t;

static const int decode_queue_size = 16;
static decode_queue_t decode_queue[decode_queue_size];
static int decode_queue_start = 0;
static int decode_queue_end = 0;

static void *decode_thread_run(void *arg);

#endif

static void do_init() {

	aomDecoder = aom_codec_av1_dx();

	aom_codec_dec_cfg_t cfg;
#ifdef __EMSCRIPTEN_PTHREADS__
	int cores = emscripten_num_logical_cores();
	if (cores > 4) {
		// we only prelaunched enough threads for 4
		cores = 4;
	}
	printf("libaom will use up to %d cores\n", cores);
	cfg.threads = cores;
#else
	cfg.threads = 0;
#endif
	cfg.w = 0; // ???
	cfg.h = 0;
	cfg.allow_lowbitdepth = 1;
	aom_codec_dec_init(&aomContext, aomDecoder, &cfg, 0);
}

void ogv_video_decoder_init() {
#ifdef __EMSCRIPTEN_PTHREADS__
	int ret = pthread_create(&decode_thread, NULL, decode_thread_run, NULL);
	if (ret) {
		printf("Error launching decode thread: %d\n", ret);
	}
	pthread_mutex_init(&decode_mutex, NULL);
	pthread_cond_init(&ping_cond, NULL);
#else
  do_init();
#endif
}

int ogv_video_decoder_async() {
#ifdef __EMSCRIPTEN_PTHREADS__
	return 1;
#else
  return 0;
#endif
}

void ogv_video_decoder_destroy() {
	// should tear instance down, but meh
#ifdef __EMSCRIPTEN_PTHREADS__
	pthread_exit(NULL);
#endif
}

int ogv_video_decoder_process_header(const char *data, size_t data_len) {
	// no header packets for AV1
	return 0;
}

static void process_frame_decode(const char *data, size_t data_len) {
	aom_codec_decode(&aomContext, (const uint8_t *)data, data_len, NULL);
	// @todo check return value
	aom_codec_decode(&aomContext, NULL, 0, NULL);
}

static int process_frame_return() {
	aom_codec_iter_t iter = NULL;
	aom_image_t *image = NULL;
	int foundImage = 0;
	while ((image = aom_codec_get_frame(&aomContext, &iter))) {
		// is it possible to get more than one at a time? ugh
		// @fixme can we have multiples really? how does this worky
		if (foundImage) {
			// skip for now
			continue;
		}
		foundImage = 1;

		int chromaWidth, chromaHeight;
		switch(image->fmt) {
			case AOM_IMG_FMT_I420:
				chromaWidth = image->w >> 1;
				chromaHeight = image->h >> 1;
				break;
			case AOM_IMG_FMT_I422:
				chromaWidth = image->w >> 1;
				chromaHeight = image->h;
				break;
			case AOM_IMG_FMT_I444:
				chromaWidth = image->w;
				chromaHeight = image->h;
				break;
			default:
				//printf("Skipping frame with unknown picture type %d\n", (int)image->fmt);
				return 0;
		}
		ogvjs_callback_frame(image->planes[0], image->stride[0],
							 image->planes[1], image->stride[1],
							 image->planes[2], image->stride[2],
							 image->w, image->h,
							 chromaWidth, chromaHeight);
	}
	return foundImage;
}

#ifdef __EMSCRIPTEN_PTHREADS__

// Send to background worker, then wake main thread on callback
int ogv_video_decoder_process_frame(const char *data, size_t data_len) {
	pthread_mutex_lock(&decode_mutex);

	decode_queue[decode_queue_end].data = data;
	decode_queue[decode_queue_end].data_len = data_len;
	decode_queue_end = (decode_queue_end + 1) % decode_queue_size;

	pthread_cond_signal(&ping_cond);
	pthread_mutex_unlock(&decode_mutex);
	return 1;
}

static void main_thread_return() {
	int ret = process_frame_return();

	pthread_mutex_lock(&decode_mutex);
	double delta = cpu_time;
	cpu_time = 0;
	busy = 0;
	pthread_cond_signal(&ping_cond);
	pthread_mutex_unlock(&decode_mutex);

	ogvjs_callback_async_complete(ret, delta);
}

static void *decode_thread_run(void *arg) {
	do_init();
	while (1) {
		pthread_mutex_lock(&decode_mutex);
		while (busy || decode_queue_end == decode_queue_start) {
			pthread_cond_wait(&ping_cond, &decode_mutex);
		}
		busy = 1;
		decode_queue_t item;
		item = decode_queue[decode_queue_start];
		decode_queue_start = (decode_queue_start + 1) % decode_queue_size;
		pthread_mutex_unlock(&decode_mutex);

		double start = emscripten_get_now();
		process_frame_decode(item.data, item.data_len);
		double delta = emscripten_get_now() - start;

		pthread_mutex_lock(&decode_mutex);
		cpu_time += delta;
		pthread_mutex_unlock(&decode_mutex);

		emscripten_async_run_in_main_runtime_thread_(EM_FUNC_SIG_V, main_thread_return);
	}
}

#else

// Single-threaded
int ogv_video_decoder_process_frame(const char *data, size_t data_len) {
	process_frame_decode(data, data_len);
	return process_frame_return();
}

#endif
