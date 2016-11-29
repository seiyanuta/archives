#include "kmalloc.h"
#include "channel.h"
#include "event.h"
#include "list.h"


static struct channel *listeners = NULL;

void listen_event(struct channel *ch, msgid_t type, uintmax_t arg) {

    struct event *new_e = kmalloc(sizeof(*new_e), KMALLOC_NORMAL);
    new_e->next  = NULL;
    new_e->type  = type;
    new_e->flags = 0;
    new_e->arg   = arg;

    ch->flags |= CHANNEL_EVENT;


    mutex_lock(&ch->receiver_lock);
    insert_into_list((struct list **) &ch->events, new_e);
    insert_into_list((struct list **) &listeners, ch);
    mutex_unlock(&ch->receiver_lock);
}


void fire_event(msgid_t type) {

    for (struct channel * ch = listeners; ch; ch = ch->next) {
        for (struct event *e = ch->events; e; e = e->next) {
            if (e->type == type) {
                e->flags |= EVENT_FIRED;
                DEBUG("fired the event 0x%x", type);
                return;
            }
        }
    }

    DEBUG("no event listener of the event 0x%x", type);
}
