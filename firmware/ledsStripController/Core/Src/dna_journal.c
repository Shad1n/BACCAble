#include "dna_journal.h"
#include <string.h>
/* Record layout, little endian, 16 bytes:
 * magic(2), sequence(4), mode(2), complement(2), checksum(2), reserved(2), commit(2).
 * Commit is programmed LAST; erased flash is 0xFF.
 */
#define MAGIC 0xD9A1u
#define COMMIT 0xA55Au
static int allowed(uint8_t m) { return m==0x00 || m==0x08 || m==0x10 || m==0x30; }
static uint16_t get16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) | ((uint32_t)get16(p+2)<<16); }
static uint16_t crc(const uint8_t *p) { uint16_t c=0xFFFF; for(unsigned i=0;i<10;i++){c^=(uint16_t)p[i]<<8; for(unsigned b=0;b<8;b++) c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);} return c; }
static int read_record(const dna_flash_ops *o,uint32_t a,uint32_t *seq,uint8_t *mode){uint8_t b[16];if(o->read(a,b,16))return -1;if(get16(b)!=MAGIC||get16(b+14)!=COMMIT)return 0;uint16_t m=get16(b+6);if(m>255||!allowed((uint8_t)m)||get16(b+8)!=(uint16_t)(~m & 0xffff)||get16(b+10)!=crc(b))return 0;*seq=get32(b+2);*mode=(uint8_t)m;return 1;}
static int newest(const dna_flash_ops *o,dna_journal_state *s,uint32_t *page,uint32_t *slot){s->valid=0;s->sequence=0;s->mode=0xFF;for(unsigned p=0;p<2;p++){uint32_t base=p?DNA_JOURNAL_PAGE_B:DNA_JOURNAL_PAGE_A;for(unsigned i=0;i<DNA_JOURNAL_PAGE_SIZE/DNA_JOURNAL_SLOT_SIZE;i++){uint32_t seq;uint8_t mode;int r=read_record(o,base+i*16,&seq,&mode);if(r<0)return -1;if(r==1&&(!s->valid||(int32_t)(seq-s->sequence)>0)){s->valid=1;s->sequence=seq;s->mode=mode;*page=base;*slot=i;}}}return 0;}
int dna_journal_load(const dna_flash_ops *o,dna_journal_state *s){uint32_t p=0,i=0;if(!o||!s||!o->read)return -1;return newest(o,s,&p,&i);}
static int blank(const dna_flash_ops *o,uint32_t a){uint8_t b[16];if(o->read(a,b,16))return -1;for(unsigned i=0;i<16;i++)if(b[i]!=0xff)return 0;return 1;}
static int write_record(const dna_flash_ops *o,uint32_t a,uint32_t seq,uint8_t mode){uint8_t b[16];memset(b,0xff,sizeof b);b[0]=MAGIC&255;b[1]=MAGIC>>8;for(unsigned i=0;i<4;i++)b[2+i]=(uint8_t)(seq>>(i*8));b[6]=mode;b[7]=0;b[8]=(uint8_t)~mode;b[9]=0xff;b[10]=crc(b)&255;b[11]=crc(b)>>8;for(unsigned i=0;i<7;i++)if(o->program_halfword(a+i*2,get16(b+i*2)))return -1;return o->program_halfword(a+14,COMMIT);}
int dna_journal_save(const dna_flash_ops *o,dna_journal_state *s,uint8_t mode){uint32_t page=0,slot=0;if(!o||!s||!o->read||!o->erase_page||!o->program_halfword||!allowed(mode))return -1;if(newest(o,s,&page,&slot))return -1;if(s->valid&&s->mode==mode)return 0;uint32_t dest=0; if(s->valid){for(unsigned i=slot+1;i<128;i++){int r=blank(o,page+i*16);if(r<0)return -1;if(r==1){dest=page+i*16;break;}}}else{for(unsigned p=0;p<2&&!dest;p++){uint32_t base=p?DNA_JOURNAL_PAGE_B:DNA_JOURNAL_PAGE_A;for(unsigned i=0;i<128;i++){int r=blank(o,base+i*16);if(r<0)return -1;if(r==1){dest=base+i*16;break;}}}}if(!dest){uint32_t other=(page==DNA_JOURNAL_PAGE_A)?DNA_JOURNAL_PAGE_B:DNA_JOURNAL_PAGE_A;if(o->erase_page(other))return -1;dest=other;}uint32_t next=s->valid?s->sequence+1:1;if(write_record(o,dest,next,mode))return -1;dna_journal_state verify; if(dna_journal_load(o,&verify)||!verify.valid||verify.sequence!=next||verify.mode!=mode)return -1;*s=verify;return 1;}
