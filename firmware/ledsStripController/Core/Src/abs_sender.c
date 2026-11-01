#include "abs_sender.h"
#include <string.h>
void abs_sender_start(AbsSender *s,uint8_t id){memset(s,0,sizeof(*s));s->id=id;s->status='B';}
void abs_sender_ready(AbsSender *s,uint16_t total,uint8_t kept,const uint8_t codes[][3]){
 if (!s) return;
 /* The UART wire format carries an 8-bit total count. Reject unrepresentable
  * or inconsistent results rather than silently reporting success. */
 if (total > UINT8_MAX || kept != (total > ABS_PROTO_MAX ? ABS_PROTO_MAX : total) ||
     (kept != 0u && codes == NULL)) {
  abs_sender_error(s, 'E');
  return;
 }
 s->kept=kept;s->total=total;s->status='R';
 for(unsigned i=0;i<kept;i++)memcpy(s->codes[i],codes[i],3);
}
void abs_sender_error(AbsSender *s,uint8_t reason){s->status='E';s->reason=reason;}
int abs_sender_get(const AbsSender *s,uint8_t request_id,uint8_t index,uint8_t route,uint8_t out[ABS_PROTO_FRAME]){
 if(!s||request_id!=s->id||index>s->kept)return -1;
 AbsReply r={0};r.id=s->id;r.status=s->status;r.reason=s->reason;
 r.total=s->total>255?255:(uint8_t)s->total;
 r.kept=s->kept;r.index=index;
 if(r.status=='R'&&index<s->kept)memcpy(r.code,s->codes[index],3);
 return abs_encode(out,route,&r);
}
