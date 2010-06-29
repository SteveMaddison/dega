/*
 * Copyright (c) 2009 Peter Collingbourne
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mencoder.h"

#define M_PI 3.14159265358979323846

#define WIDTH 320
#define HEIGHT 240

void VideoFrame(int frame, char *data) {
	memset(data, 255, WIDTH*(frame%HEIGHT)*3);
	memset(data+WIDTH*(frame%HEIGHT)*3, 0, WIDTH*(HEIGHT-(frame%HEIGHT))*3);
}

#define SAMPLERATE 44100
#define FPS 60
#define SPF (SAMPLERATE/FPS)

#define AUDIOHZ 500
#define AMP 64

void AudioFrame(int frame, char *data) {
	for (int i = 0; i < SPF; i++) {
		long sample = i + SPF*frame;
		data[i] = (char)(((double)AMP) * sin(((double)(sample*2*M_PI*AUDIOHZ)/(double)SAMPLERATE)));
	}
}

int main(int argc, char **argv) {
	Mencoder enc;
	char vbuf[WIDTH*HEIGHT*3];
	char abuf[SPF];
	int status;

	enc.mencoder = 0;
	enc.width = WIDTH;
	enc.height = HEIGHT;
	enc.fps = FPS;
	enc.channels = 1;
	enc.rate = SAMPLERATE;
	enc.samplesize = 1;
	enc.params = argv+1;
	enc.output = "test.avi";
	if (MencoderOpen(&enc) == -1) {
		perror("MencoderOpen");
		return 1;
	}

	for (int frame = 0; frame < 1000; frame++) {
		int rv;
		VideoFrame(frame, vbuf);
		AudioFrame(frame, abuf);
		
#define CHECKRV(fn) \
	if (rv == 0) { \
		puts("Buffer overrun!"); \
		return 1; \
	} \
	if (rv == -1) { \
		perror(fn); \
		return 1; \
	}
		rv = MencoderVideoData(&enc, vbuf);
		CHECKRV("MencoderVideoData")
		rv = MencoderAudioData(&enc, abuf);
		CHECKRV("MencoderAudioData")
#undef CHECKRV
	}

	if (MencoderClose(&enc, &status) == -1) {
		perror("MencoderClose");
		return 1;
	} else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
		printf("mencoder subprocess exited with return code %d\n", WEXITSTATUS(status));
		return WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		printf("mencoder subprocess terminated with signal %d\n", WTERMSIG(status));
		return 1;
	}
	return 0;
}
