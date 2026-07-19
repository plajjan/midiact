#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <rtmidi_c.h>

uv_loop_t *get_uv_loop(void);

void midiQ_midiQ___ext_init__() {
    // NOP
}

// ---------------------------------------------------------------------------
// Endpoint enumeration
//
// RtMidi identifies ports by index, so the Acton-facing "ref" is just the port
// index that _open() later passes to rtmidi_open_port().
// ---------------------------------------------------------------------------

static B_str port_name(RtMidiPtr device, unsigned int port) {
    int len = 0;
    rtmidi_get_port_name(device, port, NULL, &len);
    if (len <= 0) {
        return to$str("<unnamed>");
    }
    char *buf = malloc((size_t)len);
    if (!buf) {
        return to$str("<unnamed>");
    }
    if (rtmidi_get_port_name(device, port, buf, &len) < 0) {
        free(buf);
        return to$str("<unnamed>");
    }
    B_str name = to$str(buf);
    free(buf);
    return name;
}

static B_list enumerate(int inputs) {
    B_list res = B_mk_list(0);

    RtMidiPtr device = inputs ? (RtMidiPtr)rtmidi_in_create_default()
                              : (RtMidiPtr)rtmidi_out_create_default();
    if (!device) {
        return res;
    }
    if (!device->ok) {
        if (inputs) rtmidi_in_free((RtMidiInPtr)device);
        else rtmidi_out_free((RtMidiOutPtr)device);
        return res;
    }

    unsigned int count = rtmidi_get_port_count(device);
    for (unsigned int i = 0; i < count; i++) {
        B_tuple item = $NEWTUPLE(
            3,
            toB_int((int64_t)i),
            port_name(device, i),
            toB_int((int64_t)i)
        );
        B_SequenceD_listG_witness->$class->append(B_SequenceD_listG_witness, res, item);
    }

    if (inputs) rtmidi_in_free((RtMidiInPtr)device);
    else rtmidi_out_free((RtMidiOutPtr)device);
    return res;
}

B_list midiQ_midiQ__sources_raw() {
    return enumerate(1);
}

B_list midiQ_midiQ__destinations_raw() {
    return enumerate(0);
}

// ---------------------------------------------------------------------------
// Input actor
//
// RtMidi invokes the read callback on its own MIDI thread, so events are queued
// under a mutex and handed to the actor's libuv loop via uv_async. RtMidi
// already delivers one complete, parsed MIDI message per callback (running
// status resolved), so no message splitting is needed here.
// ---------------------------------------------------------------------------

enum rtmidi_event_type {
    RTMIDI_EVENT_PACKET,
    RTMIDI_EVENT_ERROR,
};

struct rtmidi_event {
    enum rtmidi_event_type type;
    uint64_t timestamp;
    uint16_t length;
    uint8_t *data;
    char *message;
    struct rtmidi_event *next;
};

struct rtmidi_input_handle {
    midiQ_midiQ_Input self;
    RtMidiInPtr in;
    uv_async_t async;
    uv_mutex_t mutex;
    struct rtmidi_event *head;
    struct rtmidi_event *tail;
    int closed;
};

static void enqueue_event(struct rtmidi_input_handle *handle, struct rtmidi_event *event) {
    uv_mutex_lock(&handle->mutex);
    if (handle->closed) {
        uv_mutex_unlock(&handle->mutex);
        free(event->data);
        free(event->message);
        free(event);
        return;
    }
    if (handle->tail) {
        handle->tail->next = event;
    } else {
        handle->head = event;
    }
    handle->tail = event;
    uv_mutex_unlock(&handle->mutex);
    uv_async_send(&handle->async);
}

static void enqueue_packet(struct rtmidi_input_handle *handle,
                           uint64_t timestamp,
                           const uint8_t *data,
                           uint16_t length) {
    struct rtmidi_event *event = calloc(1, sizeof(struct rtmidi_event));
    event->type = RTMIDI_EVENT_PACKET;
    event->timestamp = timestamp;
    event->length = length;
    event->data = malloc(length);
    memcpy(event->data, data, length);
    enqueue_event(handle, event);
}

static void midi_read_proc(double timestamp,
                           const unsigned char *message,
                           size_t size,
                           void *user_data) {
    (void)timestamp;
    struct rtmidi_input_handle *handle = user_data;
    if (!handle || !message || size == 0) {
        return;
    }
    // RtMidi's delta-time is seconds since the previous message; expose a
    // monotonic receive time instead so downstream timestamps are absolute.
    uint64_t ts = uv_hrtime();
    uint16_t length = size > UINT16_MAX ? UINT16_MAX : (uint16_t)size;
    enqueue_packet(handle, ts, (const uint8_t *)message, length);
}

static struct rtmidi_event *pop_events(struct rtmidi_input_handle *handle) {
    uv_mutex_lock(&handle->mutex);
    struct rtmidi_event *events = handle->head;
    handle->head = NULL;
    handle->tail = NULL;
    uv_mutex_unlock(&handle->mutex);
    return events;
}

static void async_delivery(uv_async_t *async) {
    struct rtmidi_input_handle *handle = async->data;
    struct rtmidi_event *event = pop_events(handle);

    while (event) {
        struct rtmidi_event *next = event->next;
        if (event->type == RTMIDI_EVENT_PACKET) {
            B_bytes raw = actBytesFromCStringLength((char *)event->data, event->length);
            handle->self->$class->_on_packet(handle->self, (int64_t)event->timestamp, raw);
        } else {
            handle->self->$class->_on_error(handle->self, to$str(event->message));
        }

        free(event->data);
        free(event->message);
        free(event);
        event = next;
    }
}

static void on_async_closed(uv_handle_t *async) {
    struct rtmidi_input_handle *handle = async->data;
    struct rtmidi_event *event = handle->head;
    while (event) {
        struct rtmidi_event *next = event->next;
        free(event->data);
        free(event->message);
        free(event);
        event = next;
    }
    uv_mutex_destroy(&handle->mutex);
    free(handle);
}

$R midiQ_midiQ_InputD__openG_local(midiQ_midiQ_Input self, $Cont C_cont, int64_t source_ref) {
    pin_actor_affinity();

    if (self->_handle != 0) {
        return $R_CONT(C_cont, B_None);
    }

    RtMidiInPtr in = rtmidi_in_create_default();
    if (!in || !in->ok) {
        const char *msg = (in && in->msg) ? in->msg : "Unable to create RtMidi input";
        self->$class->_on_error(self, to$str((char *)msg));
        if (in) rtmidi_in_free(in);
        return $R_CONT(C_cont, B_None);
    }

    struct rtmidi_input_handle *handle = calloc(1, sizeof(struct rtmidi_input_handle));
    handle->self = self;
    handle->in = in;
    uv_mutex_init(&handle->mutex);
    uv_async_init(get_uv_loop(), &handle->async, async_delivery);
    handle->async.data = handle;

    // Receive SysEx; ignore MIDI timing clock and active-sensing chatter.
    rtmidi_in_ignore_types(in, false, true, true);
    rtmidi_in_set_callback(in, midi_read_proc, handle);

    rtmidi_open_port(in, (unsigned int)source_ref, "acton midi input");
    if (!in->ok) {
        const char *msg = in->msg ? in->msg : "Unable to open RtMidi port";
        self->$class->_on_error(self, to$str((char *)msg));
        rtmidi_in_cancel_callback(in);
        rtmidi_in_free(in);
        handle->in = NULL;
        uv_close((uv_handle_t *)&handle->async, on_async_closed);
        return $R_CONT(C_cont, B_None);
    }

    self->_handle = (int64_t)(intptr_t)handle;
    self->_closed = false;
    return $R_CONT(C_cont, B_None);
}

$R midiQ_midiQ_InputD_closeG_local(midiQ_midiQ_Input self, $Cont C_cont) {
    struct rtmidi_input_handle *handle =
        (struct rtmidi_input_handle *)(intptr_t)self->_handle;
    if (!handle) {
        return $R_CONT(C_cont, B_None);
    }

    uv_mutex_lock(&handle->mutex);
    handle->closed = 1;
    uv_mutex_unlock(&handle->mutex);

    if (handle->in) {
        rtmidi_in_cancel_callback(handle->in);
        rtmidi_close_port(handle->in);
        rtmidi_in_free(handle->in);
        handle->in = NULL;
    }
    if (!uv_is_closing((uv_handle_t *)&handle->async)) {
        uv_close((uv_handle_t *)&handle->async, on_async_closed);
    }

    self->_handle = 0;
    self->_closed = true;
    return $R_CONT(C_cont, B_None);
}
