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

#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "fifo.h"
#include "mencoder.h"

int MencoderOpen(Mencoder *enc) {
	int videofds[2], audiofds[2];

	if (pipe(videofds) == -1) return -1;
	if (pipe(audiofds) == -1) return -1;

	if ((enc->pid = fork()) == 0) {
		char rawvideo[64], rawaudio[64], audiofile[32], videofile[32];
		char *cmdline[512] = { enc->mencoder ? enc->mencoder : "mencoder",
			"-demuxer", "rawvideo",
			"-rawvideo", rawvideo,
			"-audio-demuxer", "rawaudio",
			"-rawaudio", rawaudio,
			"-audiofile", audiofile };
		int cmdlinesize = 11;
		char **param = enc->params;

		close(videofds[1]);
		close(audiofds[1]);

		snprintf(rawvideo, sizeof(rawvideo), "format=0x52474218:w=%d:h=%d:fps=%f", enc->width, enc->height, enc->fps);
		snprintf(rawaudio, sizeof(rawaudio), "channels=%d:rate=%d:samplesize=%d", enc->channels, enc->rate, enc->samplesize);
		snprintf(audiofile, sizeof(audiofile), "/dev/fd/%d", audiofds[0]);
		snprintf(videofile, sizeof(videofile), "/dev/fd/%d", videofds[0]);

		while (*param) {
			cmdline[cmdlinesize++] = *(param++);
		}

		cmdline[cmdlinesize++] = "-o";
		cmdline[cmdlinesize++] = enc->output;
	
		cmdline[cmdlinesize++] = videofile;

		cmdline[cmdlinesize] = 0;
		
		execvp(cmdline[0], cmdline);
		perror(cmdline[0]);
		exit(1);
	} else {
		if (close(videofds[0]) == -1) return -1;
		if (close(audiofds[0]) == -1) return -1;

		enc->fifogroup[0] = &enc->videofifo;
		enc->fifogroup[1] = &enc->audiofifo;
		enc->fifogroup[2] = 0;

		if (FifoInit(&enc->videofifo, videofds[1], enc->fifogroup) == -1) return -1;
		if (FifoInit(&enc->audiofifo, audiofds[1], enc->fifogroup) == -1) return -1;

		return 0;
	}
}

int MencoderAudioBufSize(Mencoder *enc) {
	int samplesPerFrame = (int)round(enc->rate / enc->fps);
	return samplesPerFrame * enc->samplesize * enc->channels;
}

int MencoderVideoBufSize(Mencoder *enc) {
	return enc->width * enc->height * 3;
}

int MencoderAudioData(Mencoder *enc, void *data) {
	return FifoPush(&enc->audiofifo, data, MencoderAudioBufSize(enc));
}

int MencoderVideoData(Mencoder *enc, void *data) {
	return FifoPush(&enc->videofifo, data, MencoderVideoBufSize(enc));
}

int MencoderClose(Mencoder *enc, int *status) {
	if (FifoGroupClose(enc->fifogroup) == -1) return -1;
	if (waitpid(enc->pid, status, 0) == -1) return -1;
	return 0;
}
