#include "abs_proto.h"
#include <string.h>
static int count_ok(unsigned total,unsigned kept){return kept==((total>ABS_PROTO_MAX)?ABS_PROTO_MAX:total);}
static char h(unsigned x){return "0123456789ABCDEF"[x&15u];}
static int v(uint8_t c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static void put(uint8_t *p,uint8_t x){p[0]=h(x>>4);p[1]=h(x);}
static int get(const uint8_t *p){int a=v(p[0]),b=v(p[1]);return (a<0||b<0)?-1:(a<<4)|b;}
int abs_encode(uint8_t out[ABS_PROTO_FRAME],uint8_t route,const AbsReply *r){
 if(!out||!r||(r->status=='R'&&!count_ok(r->total,r->kept))||r->kept>(int)ABS_PROTO_MAX||r->index>r->kept||(r->status!='R'&&r->status!='B'&&r->status!='E'))return -1;
 memset(out,' ',ABS_PROTO_FRAME);out[0]=route;out[1]=ABS_PROTO_REPLY;put(out+2,r->id);out[4]=r->status;out[5]=r->reason;put(out+6,r->total);put(out+8,r->kept);put(out+10,r->index);
 if(r->status=='R'&&r->index<r->kept){for(unsigned i=0;i<3;i++)put(out+12+2*i,r->code[i]);}return 0;
}
int abs_decode(const uint8_t in[ABS_PROTO_FRAME],uint8_t route,AbsReply *r){
 if(!in||!r||in[0]!=route||in[1]!=ABS_PROTO_REPLY||in[18]!=' ')return -1;
 int id=get(in+2),total=get(in+6),kept=get(in+8),index=get(in+10);
 if(id<0||total<0||kept<0||index<0||kept>(int)ABS_PROTO_MAX||index>kept|| (in[4]!='R'&&in[4]!='B'&&in[4]!='E'))return -1;
 if(in[4]=='R'&&!count_ok((unsigned)total,(unsigned)kept))return -1;
 AbsReply tmp={0};tmp.id=id;tmp.status=in[4];tmp.reason=in[5];tmp.total=total;tmp.kept=kept;tmp.index=index;
 if(tmp.status=='R'&&tmp.index<tmp.kept){for(unsigned i=0;i<3;i++){int x=get(in+12+2*i);if(x<0)return -1;tmp.code[i]=x;}}
 else {for(unsigned i=12;i<18;i++)if(in[i]!=' ')return -1;}
 *r=tmp;return 0;
}
