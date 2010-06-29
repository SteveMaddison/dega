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
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "libvfw.h"

#define str(x) _str(x)
#define _str(x) #x

#define CHECKRV(fn) \
	if (rv != 0) { \
		printf(__FILE__ ":" str(__LINE__) ": " fn " failed: 0x%x\n", rv); \
		return 1; \
	}
#define TRY(fn, args) \
	do { \
		if (0) puts(__FILE__ ":" str(__LINE__) ": calling " #fn); \
		rv = fn args; \
		CHECKRV(#fn) \
	} while (0)
#define CHECKRV3(fn, last) \
	if (rv != 0) { \
		printf(__FILE__ ":" str(__LINE__) ": " fn " failed: 0x%x\n", rv); \
		goto free_ ## last; \
	}
#define TRY3(fn, args, last) \
	do { \
		if (0) puts(__FILE__ ":" str(__LINE__) ": calling " #fn); \
		rv = fn args; \
		CHECKRV3(#fn, last) \
	} while (0)

static void VFWVideoFormat(VFW *vfw, BITMAPINFOHEADER *bih) {
	memset(bih, 0, sizeof(*bih));
	bih->biSize = sizeof(*bih);
	bih->biWidth = vfw->width;
	bih->biHeight = vfw->height;
	bih->biPlanes = 1;
	bih->biBitCount = 24;
	bih->biCompression = BI_RGB;
//	bih->biSizeImage = 3*vfw->width*vfw->height;
	bih->biSizeImage = 0;
	bih->biXPelsPerMeter = 2048;
	bih->biYPelsPerMeter = 2048;
}

void VFWOptionsInit(VFWOptions *opts, VFW *vfw) {
	AVIFileInit();
	memset(&opts->wfxSrc, 0, sizeof(opts->wfxSrc));
	opts->wfxSrc.wFormatTag = WAVE_FORMAT_PCM;
	opts->wfxSrc.nChannels = vfw->channels;
	opts->wfxSrc.nSamplesPerSec = vfw->rate;
	opts->wfxSrc.nAvgBytesPerSec = vfw->channels*vfw->rate*vfw->samplesize;
	opts->wfxSrc.nBlockAlign = vfw->channels*vfw->samplesize;
	opts->wfxSrc.wBitsPerSample = vfw->samplesize*8;
	opts->wfxSrc.cbSize = 0;

	VFWVideoFormat(vfw, &opts->bi);

	memset(&opts->cv, 0, sizeof(opts->cv));
	opts->cv.cbSize = sizeof(opts->cv);
	acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, &opts->wfxSize);
	opts->wfx = malloc(opts->wfxSize);
	memcpy(opts->wfx, &opts->wfxSrc, sizeof(opts->wfxSrc));
}

int VFWOptionsValid(VFWOptions *opts) {
	return !!(opts->cv.dwFlags & ICMF_COMPVARS_VALID);
}

int VFWOptionsChooseAudioCodec(VFWOptions *opts) {
	ACMFORMATCHOOSE afmtc;
	int rv;
	memset(&afmtc, 0, sizeof(afmtc));
	afmtc.cbStruct = sizeof(afmtc);
	afmtc.hwndOwner = opts->parent;

	afmtc.pwfx = opts->wfx;
	afmtc.fdwStyle = ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT;
	afmtc.cbwfx = opts->wfxSize;

	afmtc.pwfxEnum = &opts->wfxSrc;
	afmtc.fdwEnum = ACM_FORMATENUMF_CONVERT;
	
	TRY(acmFormatChoose, (&afmtc));
	return 0;
}

int VFWOptionsChooseVideoCodec(VFWOptions *opts) {
	return !ICCompressorChoose(opts->parent, ICMF_CHOOSE_KEYFRAME | ICMF_CHOOSE_DATARATE, &opts->bi, 0, &opts->cv, 0);
}

static void VFWOptionsVideoCompOpts(VFWOptions *opts, AVICOMPRESSOPTIONS *co) {
	memset(co, 0, sizeof(*co));
	co->fccType = streamtypeVIDEO;
	co->fccHandler = opts->cv.fccHandler;
	co->dwKeyFrameEvery = opts->cv.lKey;
	if (co->dwKeyFrameEvery) co->dwFlags |= AVICOMPRESSF_KEYFRAMES;
	co->dwQuality = opts->cv.lQ;
	co->dwBytesPerSecond = opts->cv.lDataRate*1024;
	co->dwFlags |= AVICOMPRESSF_VALID | AVICOMPRESSF_DATARATE;
	co->lpParms = opts->cv.lpState;
	co->cbParms = opts->cv.cbState;
	co->dwInterleaveEvery = 1;
	co->dwFlags |= AVICOMPRESSF_INTERLEAVE;
}

int VFWOptionsDescribeAudioCodec(VFWOptions *opts, wchar_t *desc, int descSize) {
	int rv;
	size_t size;
	ACMFORMATTAGDETAILSW aftd;
	ACMFORMATDETAILSW afd;

	memset(&aftd, 0, sizeof(aftd));
	aftd.cbStruct = sizeof(aftd);
	aftd.dwFormatTag = opts->wfx->wFormatTag;
	TRY(acmFormatTagDetailsW, (0, &aftd, ACM_FORMATTAGDETAILSF_FORMATTAG));

	wcsncpy(desc, aftd.szFormatTag, descSize-1);
	desc[descSize-1] = 0;
	size = wcslen(aftd.szFormatTag);

	if (size+2 < descSize) {
		desc[size] = ' ';

		memset(&afd, 0, sizeof(afd));
		afd.cbStruct = sizeof(afd);
		afd.dwFormatTag = opts->wfx->wFormatTag;
		afd.pwfx = opts->wfx;
		afd.cbwfx = sizeof(WAVEFORMATEX) + opts->wfx->cbSize;
		TRY(acmFormatDetailsW, (0, &afd, ACM_FORMATDETAILSF_FORMAT));

		wcsncpy(desc+size+1, afd.szFormat, descSize-size-2);
		desc[descSize-1] = 0;
	}

	return 0;
}

int VFWOptionsDescribeVideoCodec(VFWOptions *opts, wchar_t *desc, int descSize) {
	HIC hic;
	ICINFO ici;

	if (!(opts->cv.dwFlags & ICMF_COMPVARS_VALID)) {
		wchar_t noneStr[7] = { '(', 'n', 'o', 'n', 'e', ')', 0 };
		wcsncpy(desc, noneStr, descSize-1);
		desc[descSize-1] = 0;
		return 0;
	}

	if (opts->cv.fccHandler == mmioFOURCC('D','I','B',' ')) {
		wchar_t uncompStr[13] = { 'U', 'n', 'c', 'o', 'm', 'p', 'r', 'e', 's', 's', 'e', 'd', 0 };
		wcsncpy(desc, uncompStr, descSize-1);
		desc[descSize-1] = 0;
		return 0;
	}

	memset(&ici, 0, sizeof(ici));
	ici.dwSize = sizeof(ici);

	hic = ICOpen(opts->cv.fccType, opts->cv.fccHandler, ICMODE_QUERY);
	if (!hic) {
		return 1;
	}

	if (!ICGetInfo(hic, &ici, sizeof(ici))) {
		ICClose(hic);
		return 1;
	}

	wcsncpy(desc, ici.szDescription, descSize-1);
	desc[descSize-1] = 0;

	ICClose(hic);
	return 0;
}

void VFWOptionsFini(VFWOptions *opts) {
	free(opts->wfx);
	AVIFileExit();
}

#define AUDIO_BLOCKS 1
// #define AUDIO_BLOCKS vfw->fps

int VFWOpen(VFW *vfw) {
	AVICOMPRESSOPTIONS aopts;
	int rv;

	AVIFileInit();

	int abufSize = vfw->abufSize = AUDIO_BLOCKS*VFWAudioBufSize(vfw);
	vfw->abuf = malloc(abufSize);

	vfw->audioFrame = vfw->videoFrame = 0;

	TRY3(acmStreamOpen, (&vfw->has, 0, &vfw->opts->wfxSrc, vfw->opts->wfx, 0, 0, 0, ACM_STREAMOPENF_NONREALTIME), abuf);
	TRY3(acmStreamSize, (vfw->has, abufSize, &vfw->acompSize, ACM_STREAMSIZEF_SOURCE), has);

	memset(&vfw->asi, 0, sizeof(vfw->asi));
	vfw->asi.fccType = streamtypeAUDIO;
	vfw->asi.dwScale = vfw->opts->wfx->nBlockAlign;
	vfw->asi.dwRate = vfw->opts->wfx->nAvgBytesPerSec;
	vfw->asi.dwSuggestedBufferSize = vfw->acompSize;
	vfw->asi.dwSampleSize = vfw->opts->wfx->nBlockAlign;
	strncpy(vfw->asi.szName, "Audio", 6);

	memset(&vfw->vsi, 0, sizeof(vfw->vsi));
	vfw->vsi.fccType = streamtypeVIDEO;
	vfw->vsi.fccHandler = mmioFOURCC('D','I','B',' ');
	vfw->vsi.dwScale = 1;
	vfw->vsi.dwRate = vfw->fps;
	vfw->vsi.dwSuggestedBufferSize = vfw->width * vfw->height * 3;
	vfw->vsi.rcFrame.right = vfw->width;
	vfw->vsi.rcFrame.bottom = vfw->height;
	strncpy(vfw->vsi.szName, "Video", 6);

	TRY3(AVIFileOpen, (&vfw->avf, vfw->output, OF_CREATE | OF_WRITE, 0), has);
	TRY3(AVIFileCreateStream, (vfw->avf, &vfw->astream, &vfw->asi), avf);
	TRY3(AVIFileCreateStream, (vfw->avf, &vfw->vstream, &vfw->vsi), astream);

	TRY3(AVIStreamSetFormat, (vfw->astream, 0, vfw->opts->wfx, sizeof(WAVEFORMATEX) + vfw->opts->wfx->cbSize), vstream);

	vfw->acomp = malloc(vfw->acompSize);
	
	memset(&vfw->ash, 0, sizeof(vfw->ash));
	vfw->ash.cbStruct = sizeof(vfw->ash);
	vfw->ash.pbSrc = vfw->abuf;
	vfw->ash.cbSrcLength = abufSize;
	vfw->ash.pbDst = vfw->acomp;
	vfw->ash.cbDstLength = vfw->acompSize;
	TRY3(acmStreamPrepareHeader, (vfw->has, &vfw->ash, 0), acomp);
	
	VFWOptionsVideoCompOpts(vfw->opts, &aopts);
	TRY3(AVIMakeCompressedStream, (&vfw->vsComp, vfw->vstream, &aopts, 0), ash);

	VFWVideoFormat(vfw, &vfw->bi);
	TRY3(AVIStreamSetFormat, (vfw->vsComp, 0, &vfw->bi, sizeof(vfw->bi)), vsComp);

	TRY3(AVIFileEndRecord, (vfw->avf), vsComp);

	return 0;

free_vsComp: 
	AVIStreamRelease(vfw->vsComp);
free_ash:
	acmStreamUnprepareHeader(vfw->has, &vfw->ash, 0);
free_acomp:
	free(vfw->acomp);
free_vstream:
	AVIStreamRelease(vfw->vstream);
free_astream:
	AVIStreamRelease(vfw->astream);
free_avf:
	AVIFileRelease(vfw->avf);
free_has:
	acmStreamClose(vfw->has, 0);
free_abuf:
	free(vfw->abuf);

	AVIFileExit();
	return 1;
}

int VFWAudioData(VFW *vfw, void *data) {
	int rv;
	int block = vfw->audioFrame++%AUDIO_BLOCKS;
	int bufSize = VFWAudioBufSize(vfw);
	void *bufDest = vfw->abuf + block*bufSize;
	memcpy(bufDest, data, bufSize);

	if (block == AUDIO_BLOCKS-1) {
		TRY(acmStreamConvert, (vfw->has, &vfw->ash, ACM_STREAMCONVERTF_BLOCKALIGN));
		if (vfw->opts->wfx->nSamplesPerSec) {
			TRY(AVIStreamWrite, (vfw->astream, vfw->opts->wfx->nSamplesPerSec*(vfw->audioFrame - AUDIO_BLOCKS)/vfw->fps, vfw->opts->wfx->nSamplesPerSec*vfw->audioFrame/vfw->fps - vfw->opts->wfx->nSamplesPerSec*(vfw->audioFrame - AUDIO_BLOCKS)/vfw->fps, vfw->acomp, vfw->ash.cbDstLengthUsed, AVIIF_KEYFRAME, 0, 0));
		} else {
			TRY(AVIStreamWrite, (vfw->astream, vfw->audioFrame-AUDIO_BLOCKS, AUDIO_BLOCKS, vfw->acomp, vfw->ash.cbDstLengthUsed, AVIIF_KEYFRAME, 0, 0));
		}
	}

	if (vfw->audioFrame == vfw->videoFrame) {
		TRY(AVIFileEndRecord, (vfw->avf));
	}

	return 0;
}

int VFWVideoData(VFW *vfw, void *data) {
	int rv;
	TRY(AVIStreamWrite, (vfw->vsComp, vfw->videoFrame++, 1, data, VFWVideoBufSize(vfw), 0, 0, 0));
	if (vfw->audioFrame == vfw->videoFrame) {
		TRY(AVIFileEndRecord, (vfw->avf));
	}
	return 0;
}

int VFWAudioBufSize(VFW *enc) {
	int samplesPerFrame = (int)round(enc->rate / (double)enc->fps);
	return samplesPerFrame * enc->samplesize * enc->channels;
}

int VFWVideoBufSize(VFW *enc) {
	return enc->width * enc->height * 3;
}

int VFWClose(VFW *vfw) {
	int rv;
	acmStreamUnprepareHeader(vfw->has, &vfw->ash, 0);
	if (vfw->audioFrame % AUDIO_BLOCKS != 0) {
		int framesInBuf = vfw->audioFrame % AUDIO_BLOCKS;
		vfw->ash.cbSrcLength = VFWAudioBufSize(vfw) * framesInBuf;
		TRY(acmStreamPrepareHeader, (vfw->has, &vfw->ash, 0));
		TRY(acmStreamConvert, (vfw->has, &vfw->ash, 0));
		if (vfw->opts->wfx->nSamplesPerSec) {
			TRY(AVIStreamWrite, (vfw->astream, vfw->opts->wfx->nSamplesPerSec*(vfw->audioFrame - framesInBuf)/AUDIO_BLOCKS, vfw->opts->wfx->nSamplesPerSec*framesInBuf/AUDIO_BLOCKS, vfw->acomp, vfw->ash.cbDstLengthUsed, AVIIF_KEYFRAME, 0, 0));
		} else {
			TRY(AVIStreamWrite, (vfw->astream, vfw->audioFrame - framesInBuf, framesInBuf, vfw->acomp, vfw->ash.cbDstLengthUsed, AVIIF_KEYFRAME, 0, 0));
		}
		TRY(acmStreamUnprepareHeader, (vfw->has, &vfw->ash, 0));
	}
	acmStreamClose(vfw->has, 0);
	free(vfw->abuf);
	free(vfw->acomp);
	AVIStreamRelease(vfw->vsComp);
	AVIStreamRelease(vfw->vstream);
	AVIStreamRelease(vfw->astream);
	AVIFileRelease(vfw->avf);
	AVIFileExit();
	return 0;
}
