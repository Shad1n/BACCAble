#include "abs_receiver.h"
#include <string.h>
void abs_receiver_init(AbsReceiver *r,uint8_t id){memset(r,0,sizeof(*r));r->id=id;r->status='B';}
int abs_receiver_accept(AbsReceiver *r,const uint8_t frame[ABS_PROTO_FRAME],uint8_t route){
 AbsReply x;if(abs_decode(frame,route,&x))return -1;
 if(x.id!=r->id)return -2;
 if(r->status=='E'||(r->status=='R'&&r->next==r->kept&&r->total_set))return -2;
 if(x.status=='E'){r->status='E';r->reason=x.reason;return -3;}
 if(x.status=='B')return 0;
 if(x.kept>x.total||x.index!=r->next)return -1;
 if(r->total_set&&(r->total!=x.total||r->kept!=x.kept))return -1;
 if(x.index<x.kept){
   if(!r->total_set){r->total=x.total;r->kept=x.kept;r->total_set=1;}
   memcpy(r->codes[r->next],x.code,3);r->next++;
   if(r->next==r->kept){r->status='R';return 2;}
   return 1;
 }
 if(x.kept==0&&!r->total_set){r->total=x.total;r->kept=0;r->total_set=1;r->status='R';return 2;}
 return -1;
}
