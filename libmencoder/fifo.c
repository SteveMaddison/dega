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

#define HEAD(fifo) ((fifo)->Head%FIFO_ELEMS)
#define TAIL(fifo) ((fifo)->Tail%FIFO_ELEMS)
#define HEAD_ELEM(fifo) ((fifo)->Elems[HEAD(fifo)])
#define TAIL_ELEM(fifo) ((fifo)->Elems[TAIL(fifo)])
#define EMPTY(fifo) ((fifo)->Head == ((fifo)->Tail+1)%(FIFO_ELEMS*2))
#define FULL(fifo) (((fifo)->Head+FIFO_ELEMS)%(FIFO_ELEMS*2) == ((fifo)->Tail+1)%(FIFO_ELEMS*2))

int FifoInit(Fifo *fifo, int fd, Fifo **group) {
	int fl;
	fifo->Head = 1;
	fifo->Tail = 0;
	fifo->fd = fd;
	fifo->MyGroup = group;
	fifo->Open = 1;
	fl = fcntl(fd, F_GETFL);
	if (fl == -1) return -1;
	return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static int TryWrite(int fd, void *data, int size) {
	while (size != 0) {
		int written = write(fd, data, size);
		if (written == -1) {
			if (errno == EAGAIN) {
				size /= 2;
			} else {
				return -1;
			}
		} else {
			return written;
		}
	}
	return 0;
}

static int FifoPop(Fifo *fifo) {
	struct FifoElem *cur;
	if (EMPTY(fifo)) return 0;

	cur = &HEAD_ELEM(fifo);
	free(cur->DataStart);
	fifo->Head = (fifo->Head+1)%(FIFO_ELEMS*2);
	return 1;
}

static int FifoFlushOnce(Fifo *fifo) {
	int rv;
	if (EMPTY(fifo)) return 0;
	do {
		struct FifoElem *cur = &HEAD_ELEM(fifo);
		int written = TryWrite(fifo->fd, cur->DataCur, cur->Remaining);
		if (written == -1) {
			return -1;
		}
		rv += written;
		
		if (written == cur->Remaining) {
			FifoPop(fifo);
			continue;
		} else {
			cur->DataCur += written;
			cur->Remaining -= written;
		}
	} while (0);
	return rv;
}

static int FifoGroupFlushOnce(Fifo **group, struct timeval *timeout, int closing) {
	int max = -1, rv = 0;
	Fifo **cgroup = group;
	fd_set fds;
	FD_ZERO(&fds);

	while (*cgroup) {
		int fd = (*cgroup)->fd;
		if (!EMPTY(*cgroup)) {
			FD_SET(fd, &fds);
			if (fd > max) max = fd;
		} else if (closing && (*cgroup)->Open) {
			if (close((*cgroup)->fd) == -1) {
				return -1;
			}
			(*cgroup)->Open = 0;
		}
		cgroup++;
	}

	if (max != -1) {
		rv = select(max+1, 0, &fds, 0, timeout);

		if (rv) {
			cgroup = group;
			while (*cgroup) {
				if (FD_ISSET((*cgroup)->fd, &fds)) {
					int frv = FifoFlushOnce(*cgroup);
					if (frv == -1) {
						return -1;
					}
					if (closing && EMPTY(*cgroup)) {
						close((*cgroup)->fd);
						(*cgroup)->Open = 0;
					}
				}
				cgroup++;
			}
		}
	}

	return rv;
}

static int FifoFlush(Fifo *fifo) {
	int rv = 0;
	
	while (1) {
		if (FULL(fifo) && fifo->MyGroup) {
			struct timeval tv;
			int flushed;
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			flushed = FifoGroupFlushOnce(fifo->MyGroup, &tv, 0);
			if (flushed == -1) {
				return -1;
			} else if (flushed == 0) {
				puts("Unable to flush any fifos (may be stuck; trying again)");
			}
		} else {
			int frv = FifoFlushOnce(fifo);
			if (frv == -1) {
				return -1;
			} else {
				rv += frv;
			}
			break;
		}
	}

	return rv;
}

int FifoPush(Fifo *fifo, void *data, int size) {
	struct FifoElem *cur;

	int rv = FifoFlush(fifo);
	if (rv == -1) {
		return -1;
	}

	if (EMPTY(fifo)) {
		int newSize = TryWrite(fifo->fd, data, size);
		if (newSize == -1) {
			return -1;
		} else if (newSize == size) {
			return 1;
		} else {
			data += newSize;
			size -= newSize;
		}
	} else if (FULL(fifo)) {
		return 0;
	}

	void *dataCopy = malloc(size);
	memcpy(dataCopy, data, size);

	fifo->Tail = (fifo->Tail+1)%(FIFO_ELEMS*2);
	cur = &TAIL_ELEM(fifo);
	cur->DataCur = cur->DataStart = dataCopy;
	cur->Remaining = size;

	return 1;
}

int FifoGroupClose(Fifo **group) {
	int rv = 1;
	while (1) {
		Fifo **cgroup = group;
		char allEmpty = 1;
		struct timeval tv;
		int flushed;

		while (*cgroup) {
			if (!EMPTY(*cgroup)) {
				allEmpty = 0;
				break;
			}
			cgroup++;
		}
		if (allEmpty) break;

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		flushed = FifoGroupFlushOnce(group, &tv, 1);
		if (flushed == -1) return -1;
		if (flushed == 0) break;
	}
	while (*group) {
		while (FifoPop(*group)) { rv = 0; } // free any entries that we were unable to write
		if ((*group)->Open && close((*group)->fd)==-1) return -1;
		group++;
	}
	return rv;
}

int FifoClose(Fifo *fifo) {
	Fifo *group[2] = { fifo, 0 };
	if (fifo->MyGroup) return 0;
	return FifoGroupClose(group);
}
