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
#include <stdio.h>

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static inline void swap(unit_t *a, unit_t *b)
{
    unit_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void split(unit_t *I,unit_t *V,unit_t start,unit_t len,unit_t h)
{
    if (len<16) {
        unit_t j;
        for(unit_t k=start; k<start+len; k+=j) {
            j=1;
            unit_t x=V[I[k]+h];
            for(unit_t i=1; k+i<start+len; i++) {
                if(V[I[k+i]+h]<x) {
                    x=V[I[k+i]+h];
                    j=0;
                };
                if(V[I[k+i]+h]==x) {
                    swap(&I[k+j], &I[k+i]);
                    j++;
                };
            };
            for(unit_t i=0; i<j; i++) V[I[k+i]]=k+j-1;
            if(j==1) I[k]=-1;
        };
        return;
    };

    unit_t x=V[I[start+len/2]+h];
    unit_t jj=0;
    unit_t kk=0;
    for(unit_t i=start; i<start+len; i++) {
        if(V[I[i]+h]<x) jj++;
        if(V[I[i]+h]==x) kk++;
    };
    jj+=start;
    kk+=jj;

    unit_t i=start;
    unit_t j=0;
    unit_t k=0;
    while(i<jj) {
        if(V[I[i]+h]<x) {
            i++;
        } else if(V[I[i]+h]==x) {
            swap(&I[i], &I[jj+j]);
            j++;
        } else {
            swap(&I[i], &I[kk+k]);
            k++;
        };
    };

    while(jj+j<kk) {
        if(V[I[jj+j]+h]==x) {
            j++;
        } else {
            swap(&I[jj+j], &I[kk+k]);
            k++;
        };
    };

    if(jj>start) split(I,V,start,jj-start,h);

    for(i=0; i<kk-jj; i++) V[I[jj+i]]=kk-1;
    if(jj==kk-1) I[jj]=-1;

    if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(unit_t *I,unit_t *V,const uint8_t *before,unit_t beforesize) {
    unit_t buckets[256];

    for(unit_t i=0; i<256; i++) buckets[i]=0;
    for(unit_t i=0; i<beforesize; i++) buckets[before[i]]++;
    for(unit_t i=1; i<256; i++) buckets[i]+=buckets[i-1];
    for(unit_t i=255; i>0; i--) buckets[i]=buckets[i-1];
    buckets[0]=0;

    for(unit_t i=0; i<beforesize; i++) I[++buckets[before[i]]]=i;
    I[0]=beforesize;
    for(unit_t i=0; i<beforesize; i++) V[i]=buckets[before[i]];
    V[beforesize]=0;
    for(unit_t i=1; i<256; i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
    I[0]=-1;

    for(unit_t h=1; I[0] != -(beforesize+1); h+=h) {
        unit_t len=0;
        unit_t i;
        for(i=0; i<beforesize+1;) {
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

    for(unit_t i=0; i<beforesize+1; i++) I[V[i]]=i;
}

static unit_t matchlen(const uint8_t *before,unit_t beforesize,const uint8_t *after,unit_t aftersize) {
    unit_t i;

    for(i=0; (i<beforesize)&&(i<aftersize); i++)
        if(before[i]!=after[i]) break;

    return i;
}

static unit_t search(const unit_t *I,const uint8_t *before,unit_t beforesize,
                      const uint8_t *after,unit_t aftersize,unit_t st,unit_t en,unit_t *pos) {
    if(en-st<2) {
        unit_t x=matchlen(before+I[st],beforesize-I[st],after,aftersize);
        unit_t y=matchlen(before+I[en],beforesize-I[en],after,aftersize);

        if(x>y) {
            *pos=I[st];
            return x;
        } else {
            *pos=I[en];
            return y;
        }
    };

    unit_t x=st+(en-st)/2;
    if(memcmp(before+I[x],after,MIN(beforesize-I[x],aftersize))<0) {
        return search(I,before,beforesize,after,aftersize,x,en,pos);
    } else {
        return search(I,before,beforesize,after,aftersize,st,x,pos);
    };
}

static void offtout(unit_t x,uint8_t *buf) {
    // The original code saves x as the absolute value with the
    // msb being the sign bit. This compresses very slightly better.
    memcpy(buf, &x, sizeof(unit_t));
}

static unit_t writedata(struct bsdiff_stream* stream, const void* buffer, unit_t length) {
    unit_t result = 0;

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
    const uint8_t* before;
    unit_t beforesize;
    const uint8_t* after;
    unit_t aftersize;
    struct bsdiff_stream* stream;
    unit_t *I;
    uint8_t *buffer;
};

static int bsdiff_internal(const struct bsdiff_request req)
{
    unit_t *V=req.stream->malloc((req.beforesize+1)*sizeof(unit_t));
    if(V==NULL)
        return -1;
    unit_t *I = req.I;

    qsufsort(I,V,req.before,req.beforesize);
    req.stream->free(V);

    uint8_t *buffer = req.buffer;

    /* Compute the differences, writing ctrl as we go */
    unit_t frank_old_offset = 0;
    unit_t frank_new_offset = 0;

    unit_t scan=0;
    unit_t len=0;
    unit_t pos=0;
    unit_t lastscan=0;
    unit_t lastpos=0;
    unit_t lastoffset=0;
    while (scan<req.aftersize) {
        unit_t beforescore=0;
        scan += len;
        for(unit_t scsc=scan; scan<req.aftersize; scan++) {
            // Search for the string of everything that's left
            // in the "after" file using the entire contents of
            // the "before" file, and store the place where a
            // substring match was found in pos,len.
            len=search(I,req.before,req.beforesize,req.after+scan,req.aftersize-scan,
                       0,req.beforesize,&pos);

            // From where we started to the length that was matched,
            // if we're still in the "before" file AND the character is
            // an exact match, bump the beforescore.
            for(; scsc<scan+len; scsc++)
                if((scsc+lastoffset<req.beforesize) &&
                        (req.before[scsc+lastoffset] == req.after[scsc]))
                    beforescore++;

            // If there was a match and it was exact (len == beforescore) OR
            // there were more than 8 mismatches, then break
            if(((len==beforescore) && (len!=0)) ||
                    (len>beforescore+8)) break;

            // If we're still in the "before" file AND the first character in
            // the match is exact, then decrement the before score.
            if((scan+lastoffset<req.beforesize) &&
                    (req.before[scan+lastoffset] == req.after[scan]))
                beforescore--;
        }

        // We either hit the end of the "after" file or we made an exact
        // match to the before file or there were more than 8 mismatches.

        if((len!=beforescore) || (scan==req.aftersize)) {
            // If not an exact match OR we reached the end of the "after" file.

            // Figure out how many characters to insert from the "before" file
            unit_t lenf=0;

            // For from the last position we were at in the "after" file to where
            // we are now. Make sure that we don't run out of bytes in the before file.
            for(unit_t i=0, s=0, Sf=0; (lastscan+i<scan)&&(lastpos+i<req.beforesize);) {
                // If an exact match, increment s
                if(req.before[lastpos+i]==req.after[lastscan+i]) s++;
                i++;

                // If 2*#exact_matches-total_chars > 2*#best_exact_matches-best_total_chars, then
                //  #best_exact_matches = #exact_matches; best_total_chars = total_chars
                if(s*2-i>Sf*2-lenf) {
                    Sf=s;
                    lenf=i;
                };
            };

            unit_t lenb=0;
            if(scan<req.aftersize) {
                // If not at the end
                unit_t s=0;
                unit_t Sb=0;

                // Go from 1 char before the match to the beginning of the "before" file or to
                // the last place we emitted patch data for in the after file.
                for(unit_t i=1; (scan>=lastscan+i)&&(pos>=i); i++) {
                    // If the character before the match is an exact match,
                    // increment s
                    if(req.before[pos-i]==req.after[scan-i]) s++;

                    // If 2*#exact_matches-total chars before > 2*#best_exact_matches-best_total_chars,
                    // then update the best.
                    if(s*2-i>Sb*2-lenb) {
                        Sb=s;
                        lenb=i;
                    }
                }
            }

            if(lastscan+lenf>scan-lenb) {
                unit_t overlap=(lastscan+lenf)-(scan-lenb);
                unit_t s=0;
                unit_t Ss=0;
                unit_t lens=0;
                for(unit_t i=0; i<overlap; i++) {
                    if(req.after[lastscan+lenf-overlap+i]==
                            req.before[lastpos+lenf-overlap+i]) s++;
                    if(req.after[scan-lenb+i]==
                            req.before[pos-lenb+i]) s--;
                    if(s>Ss) {
                        Ss=s;
                        lens=i+1;
                    };
                };

                lenf+=lens-overlap;
                lenb-=lens;
            };

            uint8_t buf[sizeof(unit_t) * 3];
            offtout(lenf,buf);
            offtout((scan-lenb)-(lastscan+lenf),buf+sizeof(unit_t));
            offtout((pos-lenb)-(lastpos+lenf),buf+2*sizeof(unit_t));
            printf("----\n");
            printf("Current file offsets: old=%d, new=%d\n", lastpos, lastscan);
            printf("Update %d bytes\n", lenf);
            frank_old_offset += lenf;
            frank_new_offset += lenf;

            printf("Insert %d bytes\n", (scan-lenb)-(lastscan+lenf));
            frank_new_offset += (scan-lenb)-(lastscan+lenf);

            printf("Seek in old data by %d bytes\n", (pos-lenb)-(lastpos+lenf));
            frank_old_offset += (pos-lenb)-(lastpos+lenf);

            printf("lastscan=%d, scan=%d, lastpos=%d, pos=%d, lenb=%d, lenf=%d\n",
                   lastscan, scan, lastpos, pos, lenb, lenf);
            printf("Current file offsets: old=%d, new=%d\n", pos, scan); // Use vars here

            /* Write control data */
            if (writedata(req.stream, buf, sizeof(buf)))
                return -1;

            /* Write diff data */
            for(unit_t i=0; i<lenf; i++)
                buffer[i]=req.after[lastscan+i]-req.before[lastpos+i];
            if (writedata(req.stream, buffer, lenf))
                return -1;

            /* Write extra data */
            for(unit_t i=0; i<(scan-lenb)-(lastscan+lenf); i++)
                buffer[i]=req.after[lastscan+lenf+i];
            if (writedata(req.stream, buffer, (scan-lenb)-(lastscan+lenf)))
                return -1;

            lastscan=scan-lenb;
            lastpos=pos-lenb;
            lastoffset=pos-scan;
        };
    };

    return 0;
}

int bsdiff(const uint8_t* before, unit_t beforesize, const uint8_t* after, unit_t aftersize, struct bsdiff_stream* stream) {
    struct bsdiff_request req;

    if((req.I=stream->malloc((beforesize+1)*sizeof(unit_t)))==NULL)
        return -1;

    if((req.buffer=stream->malloc(aftersize+1))==NULL) {
        stream->free(req.I);
        return -1;
    }

    req.before = before;
    req.beforesize = beforesize;
    req.after = after;
    req.aftersize = aftersize;
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
    uint8_t *before,*after;
    off_t beforesize,aftersize;
    uint8_t buf[sizeof(unit_t)];
    FILE * pf;
    struct bsdiff_stream stream;

    stream.malloc = malloc;
    stream.free = free;
    stream.write = bz2_write;

    if(argc!=4) errx(1,"usage: %s beforefile afterfile patchfile\n",argv[0]);

    /* Allocate beforesize+1 bytes instead of beforesize bytes to ensure
        that we never try to malloc(0) and get a NULL pointer */
    if(((fd=open(argv[1],O_RDONLY,0))<0) ||
            ((beforesize=lseek(fd,0,SEEK_END))==-1) ||
            ((before=malloc(beforesize+1))==NULL) ||
            (lseek(fd,0,SEEK_SET)!=0) ||
            (read(fd,before,beforesize)!=beforesize) ||
            (close(fd)==-1)) err(1,"%s",argv[1]);


    /* Allocate aftersize+1 bytes instead of aftersize bytes to ensure
        that we never try to malloc(0) and get a NULL pointer */
    if(((fd=open(argv[2],O_RDONLY,0))<0) ||
            ((aftersize=lseek(fd,0,SEEK_END))==-1) ||
            ((after=malloc(aftersize+1))==NULL) ||
            (lseek(fd,0,SEEK_SET)!=0) ||
            (read(fd,after,aftersize)!=aftersize) ||
            (close(fd)==-1)) err(1,"%s",argv[2]);

    /* Create the patch file */
    if ((pf = fopen(argv[3], "w")) == NULL)
        err(1, "%s", argv[3]);

    /* Write header (aftersize)*/
    offtout(aftersize, buf);
    if (fwrite(buf, sizeof(buf), 1, pf) != 1)
        err(1, "Failed to write header");

    stream.opaque = pf;
    if (bsdiff(before, beforesize, after, aftersize, &stream))
        err(1, "bsdiff");

    if (fclose(pf))
        err(1, "fclose");

    /* Free the memory we used */
    free(before);
    free(after);

    return 0;
}

#endif
