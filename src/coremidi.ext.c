#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <uv.h>

uv_loop_t *get_uv_loop(void);

void coremidiQ___ext_init__() {
    // NOP
}

#ifdef __APPLE__

typedef const void *CFAllocatorRef;
typedef const void *CFStringRef;
typedef const void *CFTypeRef;
typedef long CFIndex;
typedef uint32_t CFStringEncoding;
typedef unsigned char Boolean;

typedef int32_t OSStatus;
typedef unsigned long ItemCount;
typedef uint32_t MIDIObjectRef;
typedef uint32_t MIDIEndpointRef;
typedef uint32_t MIDIClientRef;
typedef uint32_t MIDIPortRef;
typedef uint64_t MIDITimeStamp;

enum {
    noErr = 0,
    kCFStringEncodingUTF8 = 0x08000100,
};

typedef struct __attribute__((packed, aligned(4))) {
    MIDITimeStamp timeStamp;
    uint16_t length;
    uint8_t data[256];
} MIDIPacket;

typedef struct {
    uint32_t numPackets;
    MIDIPacket packet[1];
} MIDIPacketList;

#define MIDIPacketNext(pkt) \
    ((const MIDIPacket *)(((uintptr_t)&(pkt)->data[(pkt)->length] + 3u) & ~(uintptr_t)3u))

typedef void (*MIDIReadProc)(const MIDIPacketList *, void *, void *);

typedef ItemCount (*fn_MIDIGetNumberOfSources)(void);
typedef ItemCount (*fn_MIDIGetNumberOfDestinations)(void);
typedef MIDIEndpointRef (*fn_MIDIGetSource)(ItemCount);
typedef MIDIEndpointRef (*fn_MIDIGetDestination)(ItemCount);
typedef OSStatus (*fn_MIDIObjectGetStringProperty)(MIDIObjectRef, CFStringRef, CFStringRef *);
typedef OSStatus (*fn_MIDIClientCreate)(CFStringRef, void *, void *, MIDIClientRef *);
typedef OSStatus (*fn_MIDIInputPortCreate)(MIDIClientRef, CFStringRef, MIDIReadProc, void *, MIDIPortRef *);
typedef OSStatus (*fn_MIDIPortConnectSource)(MIDIPortRef, MIDIEndpointRef, void *);
typedef OSStatus (*fn_MIDIPortDisconnectSource)(MIDIPortRef, MIDIEndpointRef);
typedef OSStatus (*fn_MIDIPortDispose)(MIDIPortRef);
typedef OSStatus (*fn_MIDIClientDispose)(MIDIClientRef);

typedef CFStringRef (*fn_CFStringCreateWithCString)(CFAllocatorRef, const char *, CFStringEncoding);
typedef Boolean (*fn_CFStringGetCString)(CFStringRef, char *, CFIndex, CFStringEncoding);
typedef void (*fn_CFRelease)(CFTypeRef);

static struct {
    int loaded;
    char error[256];
    void *coremidi;
    void *corefoundation;
    CFStringRef kMIDIPropertyName;

    fn_MIDIGetNumberOfSources MIDIGetNumberOfSources;
    fn_MIDIGetNumberOfDestinations MIDIGetNumberOfDestinations;
    fn_MIDIGetSource MIDIGetSource;
    fn_MIDIGetDestination MIDIGetDestination;
    fn_MIDIObjectGetStringProperty MIDIObjectGetStringProperty;
    fn_MIDIClientCreate MIDIClientCreate;
    fn_MIDIInputPortCreate MIDIInputPortCreate;
    fn_MIDIPortConnectSource MIDIPortConnectSource;
    fn_MIDIPortDisconnectSource MIDIPortDisconnectSource;
    fn_MIDIPortDispose MIDIPortDispose;
    fn_MIDIClientDispose MIDIClientDispose;

    fn_CFStringCreateWithCString CFStringCreateWithCString;
    fn_CFStringGetCString CFStringGetCString;
    fn_CFRelease CFRelease;
} cm;

static void *load_symbol(void *handle, const char *name) {
    void *symbol = dlsym(handle, name);
    if (!symbol) {
        snprintf(cm.error, sizeof(cm.error), "Unable to load CoreMIDI symbol %s", name);
    }
    return symbol;
}

static int coremidi_load(void) {
    if (cm.loaded) {
        return cm.error[0] == '\0';
    }
    cm.loaded = 1;

    cm.coremidi = dlopen(
        "/System/Library/Frameworks/CoreMIDI.framework/CoreMIDI",
        RTLD_LAZY | RTLD_LOCAL
    );
    if (!cm.coremidi) {
        snprintf(cm.error, sizeof(cm.error), "Unable to open CoreMIDI.framework");
        return 0;
    }

    cm.corefoundation = dlopen(
        "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
        RTLD_LAZY | RTLD_LOCAL
    );
    if (!cm.corefoundation) {
        snprintf(cm.error, sizeof(cm.error), "Unable to open CoreFoundation.framework");
        return 0;
    }

#define LOAD_CM(name) do { \
    cm.name = (fn_##name)load_symbol(cm.coremidi, #name); \
    if (!cm.name) return 0; \
} while (0)

#define LOAD_CF(name) do { \
    cm.name = (fn_##name)load_symbol(cm.corefoundation, #name); \
    if (!cm.name) return 0; \
} while (0)

    LOAD_CM(MIDIGetNumberOfSources);
    LOAD_CM(MIDIGetNumberOfDestinations);
    LOAD_CM(MIDIGetSource);
    LOAD_CM(MIDIGetDestination);
    LOAD_CM(MIDIObjectGetStringProperty);
    LOAD_CM(MIDIClientCreate);
    LOAD_CM(MIDIInputPortCreate);
    LOAD_CM(MIDIPortConnectSource);
    LOAD_CM(MIDIPortDisconnectSource);
    LOAD_CM(MIDIPortDispose);
    LOAD_CM(MIDIClientDispose);

    LOAD_CF(CFStringCreateWithCString);
    LOAD_CF(CFStringGetCString);
    LOAD_CF(CFRelease);

#undef LOAD_CM
#undef LOAD_CF

    cm.kMIDIPropertyName = cm.CFStringCreateWithCString(
        NULL,
        "name",
        kCFStringEncodingUTF8
    );
    if (!cm.kMIDIPropertyName) {
        snprintf(cm.error, sizeof(cm.error), "Unable to create CoreMIDI name property string");
        return 0;
    }
    return 1;
}

static CFStringRef cf_string(const char *text) {
    if (!coremidi_load()) {
        return NULL;
    }
    return cm.CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
}

static B_str endpoint_name(MIDIObjectRef obj) {
    if (!coremidi_load()) {
        return to$str(cm.error);
    }

    CFStringRef name = NULL;
    OSStatus status = cm.MIDIObjectGetStringProperty(obj, cm.kMIDIPropertyName, &name);
    if (status != noErr || name == NULL) {
        char buf[64];
        snprintf(buf, sizeof(buf), "<unnamed status=%d>", (int)status);
        return to$str(buf);
    }

    char buf[1024];
    if (!cm.CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        cm.CFRelease(name);
        return to$str("<name is not UTF-8>");
    }
    cm.CFRelease(name);
    return to$str(buf);
}

static B_list endpoint_list(int sources) {
    B_list res = B_mk_list(0);
    if (!coremidi_load()) {
        return res;
    }

    ItemCount count = sources ? cm.MIDIGetNumberOfSources()
                              : cm.MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = sources ? cm.MIDIGetSource(i)
                                           : cm.MIDIGetDestination(i);
        B_tuple item = $NEWTUPLE(
            3,
            toB_int((int64_t)i),
            endpoint_name((MIDIObjectRef)endpoint),
            toB_int((int64_t)endpoint)
        );
        B_SequenceD_listG_witness->$class->append(B_SequenceD_listG_witness, res, item);
    }
    return res;
}

B_list coremidiQ__sources_raw() {
    return endpoint_list(1);
}

B_list coremidiQ__destinations_raw() {
    return endpoint_list(0);
}

enum coremidi_event_type {
    COREMIDI_EVENT_PACKET,
    COREMIDI_EVENT_ERROR,
};

struct coremidi_event {
    enum coremidi_event_type type;
    uint64_t timestamp;
    uint16_t length;
    uint8_t *data;
    char *message;
    struct coremidi_event *next;
};

struct coremidi_input_handle {
    coremidiQ_Input self;
    MIDIClientRef client;
    MIDIPortRef port;
    MIDIEndpointRef source;
    uv_async_t async;
    uv_mutex_t mutex;
    struct coremidi_event *head;
    struct coremidi_event *tail;
    uint8_t running_status;
    int closed;
};

static void enqueue_event(struct coremidi_input_handle *handle, struct coremidi_event *event) {
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

static void enqueue_error(struct coremidi_input_handle *handle, const char *message) {
    struct coremidi_event *event = calloc(1, sizeof(struct coremidi_event));
    event->type = COREMIDI_EVENT_ERROR;
    event->message = strdup(message);
    enqueue_event(handle, event);
}

static void enqueue_packet(struct coremidi_input_handle *handle,
                           uint64_t timestamp,
                           const uint8_t *data,
                           uint16_t length) {
    struct coremidi_event *event = calloc(1, sizeof(struct coremidi_event));
    event->type = COREMIDI_EVENT_PACKET;
    event->timestamp = timestamp;
    event->length = length;
    event->data = malloc(length);
    memcpy(event->data, data, length);
    enqueue_event(handle, event);
}

static int midi_payload_len(uint8_t status) {
    if ((status >= 0x80 && status <= 0xbf) || (status >= 0xe0 && status <= 0xef)) {
        return 2;
    }
    if (status >= 0xc0 && status <= 0xdf) {
        return 1;
    }
    if (status == 0xf1 || status == 0xf3) {
        return 1;
    }
    if (status == 0xf2) {
        return 2;
    }
    if (status == 0xf6 || status == 0xf8 || status == 0xfa ||
        status == 0xfb || status == 0xfc || status == 0xfe ||
        status == 0xff) {
        return 0;
    }
    return -1;
}

static void enqueue_midi_messages(struct coremidi_input_handle *handle,
                                  uint64_t timestamp,
                                  const uint8_t *data,
                                  uint16_t length) {
    uint16_t i = 0;
    while (i < length) {
        uint8_t status;
        uint16_t status_pos = i;

        if (data[i] >= 0x80) {
            status = data[i++];
        } else if (handle->running_status != 0) {
            status = handle->running_status;
        } else {
            enqueue_packet(handle, timestamp, &data[i], 1);
            i++;
            continue;
        }

        int payload_len = midi_payload_len(status);
        if (payload_len < 0) {
            enqueue_packet(handle, timestamp, &data[status_pos], length - status_pos);
            return;
        }
        if (i + payload_len > length) {
            enqueue_packet(handle, timestamp, &data[status_pos], length - status_pos);
            return;
        }

        uint8_t msg[3];
        msg[0] = status;
        for (int n = 0; n < payload_len; n++) {
            msg[n + 1] = data[i + n];
        }
        i += payload_len;

        if (status < 0xf0) {
            handle->running_status = status;
        } else {
            handle->running_status = 0;
        }
        enqueue_packet(handle, timestamp, msg, (uint16_t)(payload_len + 1));
    }
}

static void midi_read_proc(const MIDIPacketList *packet_list,
                           void *read_proc_refcon,
                           void *src_conn_refcon) {
    (void)src_conn_refcon;
    struct coremidi_input_handle *handle = read_proc_refcon;
    if (!handle || !packet_list) {
        return;
    }

    const MIDIPacket *packet = &packet_list->packet[0];
    for (uint32_t i = 0; i < packet_list->numPackets; i++) {
        enqueue_midi_messages(handle, packet->timeStamp, packet->data, packet->length);
        packet = MIDIPacketNext(packet);
    }
}

static struct coremidi_event *pop_events(struct coremidi_input_handle *handle) {
    uv_mutex_lock(&handle->mutex);
    struct coremidi_event *events = handle->head;
    handle->head = NULL;
    handle->tail = NULL;
    uv_mutex_unlock(&handle->mutex);
    return events;
}

static void async_delivery(uv_async_t *async) {
    struct coremidi_input_handle *handle = async->data;
    struct coremidi_event *event = pop_events(handle);

    while (event) {
        struct coremidi_event *next = event->next;
        if (event->type == COREMIDI_EVENT_PACKET) {
            B_bytes raw = actBytesFromCStringLength((char *)event->data, event->length);
            handle->self->$class->_on_packet(handle->self, toB_int((int64_t)event->timestamp), raw);
        } else {
            handle->self->$class->_on_error(handle->self, to$str(event->message));
        }

        free(event->data);
        free(event->message);
        free(event);
        event = next;
    }
}

static void report_open_error(coremidiQ_Input self, const char *context, OSStatus status) {
    char msg[160];
    snprintf(msg, sizeof(msg), "%s failed with CoreMIDI OSStatus %d", context, (int)status);
    self->$class->_on_error(self, to$str(msg));
}

$R coremidiQ_InputD__openG_local(coremidiQ_Input self, $Cont C_cont, B_int source_ref) {
    pin_actor_affinity();

    if (!coremidi_load()) {
        self->$class->_on_error(self, to$str(cm.error));
        return $R_CONT(C_cont, B_None);
    }
    if (fromB_int(self->_handle) != 0) {
        return $R_CONT(C_cont, B_None);
    }

    struct coremidi_input_handle *handle = calloc(1, sizeof(struct coremidi_input_handle));
    handle->self = self;
    handle->source = (MIDIEndpointRef)fromB_int(source_ref);
    uv_mutex_init(&handle->mutex);
    uv_async_init(get_uv_loop(), &handle->async, async_delivery);
    handle->async.data = handle;

    CFStringRef client_name = cf_string("acton coremidi");
    CFStringRef port_name = cf_string("acton coremidi input");
    if (!client_name || !port_name) {
        self->$class->_on_error(self, to$str("Unable to create CoreMIDI client strings"));
        return $R_CONT(C_cont, B_None);
    }

    OSStatus status = cm.MIDIClientCreate(client_name, NULL, NULL, &handle->client);
    cm.CFRelease(client_name);
    if (status != noErr) {
        cm.CFRelease(port_name);
        report_open_error(self, "MIDIClientCreate", status);
        return $R_CONT(C_cont, B_None);
    }

    status = cm.MIDIInputPortCreate(
        handle->client,
        port_name,
        midi_read_proc,
        handle,
        &handle->port
    );
    cm.CFRelease(port_name);
    if (status != noErr) {
        cm.MIDIClientDispose(handle->client);
        report_open_error(self, "MIDIInputPortCreate", status);
        return $R_CONT(C_cont, B_None);
    }

    status = cm.MIDIPortConnectSource(handle->port, handle->source, NULL);
    if (status != noErr) {
        cm.MIDIPortDispose(handle->port);
        cm.MIDIClientDispose(handle->client);
        report_open_error(self, "MIDIPortConnectSource", status);
        return $R_CONT(C_cont, B_None);
    }

    self->_handle = toB_int((int64_t)(intptr_t)handle);
    self->_closed = B_False;
    return $R_CONT(C_cont, B_None);
}

$R coremidiQ_InputD_closeG_local(coremidiQ_Input self, $Cont C_cont) {
    struct coremidi_input_handle *handle =
        (struct coremidi_input_handle *)(intptr_t)fromB_int(self->_handle);
    if (!handle) {
        return $R_CONT(C_cont, B_None);
    }

    uv_mutex_lock(&handle->mutex);
    handle->closed = 1;
    uv_mutex_unlock(&handle->mutex);

    cm.MIDIPortDisconnectSource(handle->port, handle->source);
    cm.MIDIPortDispose(handle->port);
    cm.MIDIClientDispose(handle->client);
    if (!uv_is_closing((uv_handle_t *)&handle->async)) {
        uv_close((uv_handle_t *)&handle->async, NULL);
    }

    self->_handle = toB_int(0);
    self->_closed = B_True;
    return $R_CONT(C_cont, B_None);
}

#else

B_list coremidiQ__sources_raw() {
    return B_mk_list(0);
}

B_list coremidiQ__destinations_raw() {
    return B_mk_list(0);
}

$R coremidiQ_InputD__openG_local(coremidiQ_Input self, $Cont C_cont, B_int source_ref) {
    (void)source_ref;
    self->$class->_on_error(self, to$str("CoreMIDI is only available on macOS"));
    return $R_CONT(C_cont, B_None);
}

$R coremidiQ_InputD_closeG_local(coremidiQ_Input self, $Cont C_cont) {
    self->_handle = toB_int(0);
    self->_closed = B_True;
    return $R_CONT(C_cont, B_None);
}

#endif
