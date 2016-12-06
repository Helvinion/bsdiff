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

#include "bsdiff.h"

#include <limits.h>
#include <string.h>

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static void split(int64_t *I,int64_t *V,int64_t start,int64_t len,int64_t h)
{
    if(len<16) {
        for(int64_t k=start; k<start+len; k+=j) {
            int64_t j=1;
            x=V[I[k]+h];
            for(int64_t i=1; k+i<start+len; i++) {
                if(V[I[k+i]+h]<x) {
                    x=V[I[k+i]+h];
                    j=0;
                };
                if(V[I[k+i]+h]==x) {
                    int64_t tmp=I[k+j];
                    I[k+j]=I[k+i];
                    I[k+i]=tmp;
                    j++;
                };
            };
            for(int64_t i=0; i<j; i++) V[I[k+i]]=k+j-1;
            if(j==1) I[k]=-1;
        };
        return;
    };

    int64_t x=V[I[start+len/2]+h];
    int64_t jj=0;
    int64_t kk=0;
    for(int64_t i=start; i<start+len; i++) {
        if(V[I[i]+h]<x) jj++;
        if(V[I[i]+h]==x) kk++;
    };
    jj+=start;
    kk+=jj;

    int64_t i=start;
    int64_t j=0;
    int64_t k=0;
    while(i<jj) {
        if(V[I[i]+h]<x) {
            i++;
        } else if(V[I[i]+h]==x) {
            int64_t tmp=I[i];
            I[i]=I[jj+j];
            I[jj+j]=tmp;
            j++;
        } else {
            int64_t tmp=I[i];
            I[i]=I[kk+k];
            I[kk+k]=tmp;
            k++;
        };
    };

    while(jj+j<kk) {
        if(V[I[jj+j]+h]==x) {
            j++;
        } else {
            int64_t tmp=I[jj+j];
            I[jj+j]=I[kk+k];
            I[kk+k]=tmp;
            k++;
        };
    };

    if(jj>start) split(I,V,start,jj-start,h);

    for(i=0; i<kk-jj; i++) V[I[jj+i]]=kk-1;
    if(jj==kk-1) I[jj]=-1;

    if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(int64_t *I,int64_t *V,const uint8_t *old,int64_t oldsize) {
    int64_t buckets[256];

    for(int64_t i=0; i<256; i++) buckets[i]=0;
    for(int64_t i=0; i<oldsize; i++) buckets[old[i]]++;
    for(int64_t i=1; i<256; i++) buckets[i]+=buckets[i-1];
    for(int64_t i=255; i>0; i--) buckets[i]=buckets[i-1];
    buckets[0]=0;

    for(int64_t i=0; i<oldsize; i++) I[++buckets[old[i]]]=i;
    I[0]=oldsize;
    for(int64_t i=0; i<oldsize; i++) V[i]=buckets[old[i]];
    V[oldsize]=0;
    for(int64_t i=1; i<256; i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
    I[0]=-1;

    for(int64_t h=1; I[0] != -(oldsize+1); h+=h) {
        int64_t len=0;
        int64_t i;
        for(i=0; i<oldsize+1;) {
            if(I[i]<0) {
                len-=I[i];
                i-=I[i];
            } else {
                if(len) I[i-len]=-len;
                len=V[I[i]]+1-i;
                split(I,V,i,len,h);
                i+=len;
                len=0;
            };
        };
        if(len) I[i-len]=-len;
    };

    for(int64_t i=0; i<oldsize+1; i++) I[V[i]]=i;
}

static int64_t matchlen(const uint8_t *old,int64_t oldsize,const uint8_t *new,int64_t newsize) {
    int64_t i;

    for(i=0; (i<oldsize)&&(i<newsize); i++)
        if(old[i]!=new[i]) break;

    return i;
}

static int64_t search(const int64_t *I,const uint8_t *old,int64_t oldsize,
                      const uint8_t *new,int64_t newsize,int64_t st,int64_t en,int64_t *pos) {
    if(en-st<2) {
        int64_t x=matchlen(old+I[st],oldsize-I[st],new,newsize);
        int64_t y=matchlen(old+I[en],oldsize-I[en],new,newsize);

        if(x>y) {
            *pos=I[st];
            return x;
        } else {
            *pos=I[en];
            return y;
        }
    };

    int64_t x=st+(en-st)/2;
    if(memcmp(old+I[x],new,MIN(oldsize-I[x],newsize))<0) {
        return search(I,old,oldsize,new,newsize,x,en,pos);
    } else {
        return search(I,old,oldsize,new,newsize,st,x,pos);
    };
}

static void offtout(int64_t x,uint8_t *buf) {
    int64_t y;

    if(x<0) y=-x;
    else y=x;

    buf[0]=y%256;
    y-=buf[0];
    y=y/256;
    buf[1]=y%256;
    y-=buf[1];
    y=y/256;
    buf[2]=y%256;
    y-=buf[2];
    y=y/256;
    buf[3]=y%256;
    y-=buf[3];
    y=y/256;
    buf[4]=y%256;
    y-=buf[4];
    y=y/256;
    buf[5]=y%256;
    y-=buf[5];
    y=y/256;
    buf[6]=y%256;
    y-=buf[6];
    y=y/256;
    buf[7]=y%256;

    if(x<0) buf[7]|=0x80;
}

static int64_t writedata(struct bsdiff_stream* stream, const void* buffer, int64_t length) {
    int64_t result = 0;

    while (length > 0) {
        const int smallsize = (int)MIN(length, INT_MAX);
        const int writeresult = stream->write(stream, buffer, smallsize);
        if (writeresult == -1) {
            return -1;
        }

        result += writeresult;
        length -= smallsize;
        buffer = (uint8_t*)buffer + smallsize;
    }

    return result;
}

struct bsdiff_request {
    const uint8_t* old;
    int64_t oldsize;
    const uint8_t* new;
    int64_t newsize;
    struct bsdiff_stream* stream;
    int64_t *I;
    uint8_t *buffer;
};

static int bsdiff_internal(const struct bsdiff_request req)
{
    int64_t *V=req.stream->malloc((req.oldsize+1)*sizeof(int64_t));
    if(V==NULL)
        return -1;
    int64_t *I = req.I;

    qsufsort(I,V,req.old,req.oldsize);
    req.stream->free(V);

    uint8_t *buffer = req.buffer;

    /* Compute the differences, writing ctrl as we go */
    int64_t scan=0;
    int64_t len=0;
    int64_t pos=0;
    int64_t lastscan=0;
    int64_t lastpos=0;
    int64_t lastoffset=0;
    while(scan<req.newsize) {
        int64_t oldscore=0;
        for(int64_t scsc=scan+=len; scan<req.newsize; scan++) {
            len=search(I,req.old,req.oldsize,req.new+scan,req.newsize-scan,
                       0,req.oldsize,&pos);

            for(; scsc<scan+len; scsc++)
                if((scsc+lastoffset<req.oldsize) &&
                        (req.old[scsc+lastoffset] == req.new[scsc]))
                    oldscore++;

            if(((len==oldscore) && (len!=0)) ||
                    (len>oldscore+8)) break;

            if((scan+lastoffset<req.oldsize) &&
                    (req.old[scan+lastoffset] == req.new[scan]))
                oldscore--;
        };

        if((len!=oldscore) || (scan==req.newsize)) {
            int64_t s=0;
            int64_t Sf=0;
            int64_t lenf=0;
            for(int64_t i=0; (lastscan+i<scan)&&(lastpos+i<req.oldsize);) {
                if(req.old[lastpos+i]==req.new[lastscan+i]) s++;
                i++;
                if(s*2-i>Sf*2-lenf) {
                    Sf=s;
                    lenf=i;
                };
            };

            int64_t lenb=0;
            if(scan<req.newsize) {
                s=0;
                int64_t Sb=0;
                for(int64_t i=1; (scan>=lastscan+i)&&(pos>=i); i++) {
                    if(req.old[pos-i]==req.new[scan-i]) s++;
                    if(s*2-i>Sb*2-lenb) {
                        Sb=s;
                        lenb=i;
                    };
                };
            };

            if(lastscan+lenf>scan-lenb) {
                int64_t overlap=(lastscan+lenf)-(scan-lenb);
                s=0;
                int64_t Ss=0;
                int64_t lens=0;
                for(int64_t i=0; i<overlap; i++) {
                    if(req.new[lastscan+lenf-overlap+i]==
                            req.old[lastpos+lenf-overlap+i]) s++;
                    if(req.new[scan-lenb+i]==
                            req.old[pos-lenb+i]) s--;
                    if(s>Ss) {
                        Ss=s;
                        lens=i+1;
                    };
                };

                lenf+=lens-overlap;
                lenb-=lens;
            };

            uint8_t buf[8 * 3];
            offtout(lenf,buf);
            offtout((scan-lenb)-(lastscan+lenf),buf+8);
            offtout((pos-lenb)-(lastpos+lenf),buf+16);

            /* Write control data */
            if (writedata(req.stream, buf, sizeof(buf)))
                return -1;

            /* Write diff data */
            for(int64_t i=0; i<lenf; i++)
                buffer[i]=req.new[lastscan+i]-req.old[lastpos+i];
            if (writedata(req.stream, buffer, lenf))
                return -1;

            /* Write extra data */
            for(int64_t i=0; i<(scan-lenb)-(lastscan+lenf); i++)
                buffer[i]=req.new[lastscan+lenf+i];
            if (writedata(req.stream, buffer, (scan-lenb)-(lastscan+lenf)))
                return -1;

            lastscan=scan-lenb;
            lastpos=pos-lenb;
            lastoffset=pos-scan;
        };
    };

    return 0;
}

int bsdiff(const uint8_t* old, int64_t oldsize, const uint8_t* new, int64_t newsize, struct bsdiff_stream* stream) {
    struct bsdiff_request req;

    if((req.I=stream->malloc((oldsize+1)*sizeof(int64_t)))==NULL)
        return -1;

    if((req.buffer=stream->malloc(newsize+1))==NULL) {
        stream->free(req.I);
        return -1;
    }

    req.old = old;
    req.oldsize = oldsize;
    req.new = new;
    req.newsize = newsize;
    req.stream = stream;

    int result = bsdiff_internal(req);

    stream->free(req.buffer);
    stream->free(req.I);

    return result;
}

#if defined(BSDIFF_EXECUTABLE)

#include <sys/types.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int bz2_write(struct bsdiff_stream* stream, const void* buffer, int size) {
    FILE *fp = (FILE*) stream->opaque;
    int written = fwrite(buffer, 1, size, fp);
    if (written != size)
        return -1;
    return 0;
}

int main(int argc,char *argv[]) {
    int fd;
    uint8_t *old,*new;
    off_t oldsize,newsize;
    uint8_t buf[8];
    FILE * pf;
    struct bsdiff_stream stream;

    stream.malloc = malloc;
    stream.free = free;
    stream.write = bz2_write;

    if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

    /* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
        that we never try to malloc(0) and get a NULL pointer */
    if(((fd=open(argv[1],O_RDONLY,0))<0) ||
            ((oldsize=lseek(fd,0,SEEK_END))==-1) ||
            ((old=malloc(oldsize+1))==NULL) ||
            (lseek(fd,0,SEEK_SET)!=0) ||
            (read(fd,old,oldsize)!=oldsize) ||
            (close(fd)==-1)) err(1,"%s",argv[1]);


    /* Allocate newsize+1 bytes instead of newsize bytes to ensure
        that we never try to malloc(0) and get a NULL pointer */
    if(((fd=open(argv[2],O_RDONLY,0))<0) ||
            ((newsize=lseek(fd,0,SEEK_END))==-1) ||
            ((new=malloc(newsize+1))==NULL) ||
            (lseek(fd,0,SEEK_SET)!=0) ||
            (read(fd,new,newsize)!=newsize) ||
            (close(fd)==-1)) err(1,"%s",argv[2]);

    /* Create the patch file */
    if ((pf = fopen(argv[3], "w")) == NULL)
        err(1, "%s", argv[3]);

    /* Write header (newsize)*/
    offtout(newsize, buf);
    if (fwrite(buf, sizeof(buf), 1, pf) != 1)
        err(1, "Failed to write header");

    stream.opaque = pf;
    if (bsdiff(old, oldsize, new, newsize, &stream))
        err(1, "bsdiff");

    if (fclose(pf))
        err(1, "fclose");

    /* Free the memory we used */
    free(old);
    free(new);

    return 0;
}

#endif
