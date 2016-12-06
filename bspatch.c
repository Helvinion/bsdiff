/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bspatch.h"

static int64_t offtin(uint8_t *buf)
{
	int64_t y;

	y=buf[7]&0x7F;
	y=y*256;y+=buf[6];
	y=y*256;y+=buf[5];
	y=y*256;y+=buf[4];
	y=y*256;y+=buf[3];
	y=y*256;y+=buf[2];
	y=y*256;y+=buf[1];
	y=y*256;y+=buf[0];

	if(buf[7]&0x80) y=-y;

	return y;
}

int bspatch(const uint8_t* before, int64_t beforesize, uint8_t* after, int64_t aftersize, struct bspatch_stream* stream)
{
	uint8_t buf[8];
    int64_t beforepos,afterpos;
	int64_t ctrl[3];
	int64_t i;

    beforepos=0;afterpos=0;
    while(afterpos<aftersize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			if (stream->read(stream, buf, 8))
				return -1;
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
        if(afterpos+ctrl[0]>aftersize)
			return -1;

		/* Read diff string */
        if (stream->read(stream, after + afterpos, ctrl[0]))
			return -1;

        /* Add before data to diff string */
		for(i=0;i<ctrl[0];i++)
            if((beforepos+i>=0) && (beforepos+i<beforesize))
                after[afterpos+i]+=before[beforepos+i];

		/* Adjust pointers */
        afterpos+=ctrl[0];
        beforepos+=ctrl[0];

		/* Sanity-check */
        if(afterpos+ctrl[1]>aftersize)
			return -1;

		/* Read extra string */
        if (stream->read(stream, after + afterpos, ctrl[1]))
			return -1;

		/* Adjust pointers */
        afterpos+=ctrl[1];
        beforepos+=ctrl[2];
	};

	return 0;
}

#if defined(BSPATCH_EXECUTABLE)

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static int bz2_read(const struct bspatch_stream* stream, void* buffer, int length)
{
    FILE *fp = (FILE*) stream->opaque;
    if (fread(buffer, 1, length, fp) == length)
        return 0;
    else
        return -1;
}

int main(int argc,char * argv[])
{
	FILE * f;
	int fd;
	uint8_t header[24];
    uint8_t *before, *after;
    int64_t beforesize, aftersize;
	struct bspatch_stream stream;
	struct stat sb;

    if(argc!=4) errx(1,"usage: %s beforefile afterfile patchfile\n",argv[0]);

	/* Open patch file */
	if ((f = fopen(argv[3], "r")) == NULL)
		err(1, "fopen(%s)", argv[3]);

	/* Read header */
    if (fread(header, 1, 8, f) != 8) {
		if (feof(f))
			errx(1, "Corrupt patch\n");
		err(1, "fread(%s)", argv[3]);
	}

	/* Read lengths from header */
    aftersize=offtin(header);
    if(aftersize<0)
		errx(1,"Corrupt patch\n");

	/* Close patch file and re-open it via libbzip2 at the right places */
	if(((fd=open(argv[1],O_RDONLY,0))<0) ||
        ((beforesize=lseek(fd,0,SEEK_END))==-1) ||
        ((before=malloc(beforesize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
        (read(fd,before,beforesize)!=beforesize) ||
		(fstat(fd, &sb)) ||
		(close(fd)==-1)) err(1,"%s",argv[1]);
    if((after=malloc(aftersize+1))==NULL) err(1,NULL);

	stream.read = bz2_read;
    stream.opaque = f;
    if (bspatch(before, beforesize, after, aftersize, &stream))
		errx(1, "bspatch");

	/* Clean up the bzip2 reads */
	fclose(f);

    /* Write the after file */
	if(((fd=open(argv[2],O_CREAT|O_TRUNC|O_WRONLY,sb.st_mode))<0) ||
        (write(fd,after,aftersize)!=aftersize) || (close(fd)==-1))
		err(1,"%s",argv[2]);

    free(after);
    free(before);

	return 0;
}

#endif
