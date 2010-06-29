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

#ifndef LIBVFW_H
#define LIBVFW_H

#include <vfw.h>
#include "mmreg.h"
#include "msacm.h"

typedef struct VFWOptions {
	HWND parent;

	WAVEFORMATEX wfxSrc;
	COMPVARS cv;
	PWAVEFORMATEX wfx;
	LONG wfxSize;
	BITMAPINFOHEADER bi;
} VFWOptions;

typedef struct VFW {
	int width, height;
	int fps;
	int channels, rate, samplesize;
	char *output;
	VFWOptions *opts;

	PAVIFILE avf;
	AVISTREAMINFO asi, vsi;
	void *abuf;
	DWORD abufSize;
	void *acomp;
	DWORD acompSize;
	PAVISTREAM astream, vstream, vsComp;
	ACMSTREAMHEADER ash;
	HACMSTREAM has;
	BITMAPINFOHEADER bi;
	int audioFrame, videoFrame;
} VFW;

void VFWOptionsInit(VFWOptions *opts, VFW *enc);
int VFWOptionsValid(VFWOptions *opts);
int VFWOptionsChooseAudioCodec(VFWOptions *opts);
int VFWOptionsChooseVideoCodec(VFWOptions *opts);
int VFWOptionsDescribeAudioCodec(VFWOptions *opts, wchar_t *desc, int descSize);
int VFWOptionsDescribeVideoCodec(VFWOptions *opts, wchar_t *desc, int descSize);
void VFWOptionsFini(VFWOptions *opts);

int VFWOpen(VFW *enc);
int VFWAudioBufSize(VFW *enc);
int VFWVideoBufSize(VFW *enc);
int VFWAudioData(VFW *enc, void *data);
int VFWVideoData(VFW *enc, void *data);
int VFWClose(VFW *enc);

#endif
