#include "abs_transport.h"
#include <string.h>
void abs_transport_init(AbsTransport *t,uint8_t id){
 memset(t,0,sizeof(*t));abs_receiver_init(&t->receiver,id);
}
int abs_transport_tick(AbsTransport *t,uint32_t now,uint8_t *index){
 if(!t||!index)return -1;
 if(t->timed_out||t->receiver.status=='E'||
    (t->receiver.total_set&&t->receiver.next==t->receiver.kept))return -1;
 if(t->poll_started&&(uint32_t)(now-t->last_poll_ms)<ABS_TRANSPORT_POLL_MS)return 0;
 if(t->polls_without_progress>=ABS_TRANSPORT_MAX_RETRIES){t->timed_out=1;return -2;}
 t->poll_started=1;t->last_poll_ms=now;t->polls_without_progress++;
 *index=t->receiver.next;return 1;
}
int abs_transport_reply(AbsTransport *t,const uint8_t frame[ABS_PROTO_FRAME],uint8_t route){
 if(!t||!frame)return -1;
 if(t->timed_out)return -2;
 int result=abs_receiver_accept(&t->receiver,frame,route);
 if(result>0)t->polls_without_progress=0;
 return result;
}
