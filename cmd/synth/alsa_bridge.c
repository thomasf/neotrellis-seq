#define _GNU_SOURCE
#include "alsa_bridge.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>

typedef struct {
    void *handle;
    int (*snd_seq_open)(void **seqp, const char *name, int streams, int mode);
    int (*snd_seq_close)(void *seq);
    int (*snd_seq_set_client_name)(void *seq, const char *name);
    int (*snd_seq_client_id)(void *seq);
    int (*snd_seq_create_simple_port)(void *seq, const char *name, unsigned int caps, unsigned int type);
    int (*snd_seq_connect_to)(void *seq, int my_port, int dest_client, int dest_port);
    int (*snd_seq_connect_from)(void *seq, int my_port, int src_client, int src_port);
    size_t (*snd_seq_client_info_sizeof)(void);
    void (*snd_seq_client_info_set_client)(void *info, int client);
    int (*snd_seq_client_info_get_client)(const void *info);
    const char *(*snd_seq_client_info_get_name)(const void *info);
    int (*snd_seq_query_next_client)(void *seq, void *info);
    size_t (*snd_seq_port_info_sizeof)(void);
    void (*snd_seq_port_info_set_client)(void *info, int client);
    void (*snd_seq_port_info_set_port)(void *info, int port);
    int (*snd_seq_port_info_get_port)(const void *info);
    const char *(*snd_seq_port_info_get_name)(const void *info);
    unsigned int (*snd_seq_port_info_get_capability)(const void *info);
    int (*snd_seq_query_next_port)(void *seq, void *info);
    int (*snd_seq_poll_descriptors_count)(void *seq, short events);
    int (*snd_seq_poll_descriptors)(void *seq, struct pollfd *pfds, unsigned int space, short events);
    int (*snd_seq_event_input)(void *seq, snd_seq_event_t **ev);
    int (*snd_seq_event_input_pending)(void *seq, int fetch_sequencer);
    int (*snd_seq_event_output_direct)(void *seq, snd_seq_event_t *ev);

    // PCM API
    int (*snd_pcm_open)(void **pcm, const char *name, int stream, int mode);
    int (*snd_pcm_set_params)(void *pcm, int format, int access, unsigned int channels, unsigned int rate, int soft_resample, unsigned int latency);
    long (*snd_pcm_writei)(void *pcm, const void *buffer, unsigned long size);
    int (*snd_pcm_recover)(void *pcm, int err, int silent);
    int (*snd_pcm_drop)(void *pcm);
    int (*snd_pcm_close)(void *pcm);
} alsa_t;

static alsa_t alsa;

int init_alsa(void) {
    if (alsa.handle) return 0; // Already initialized

    void *h = dlopen("libasound.so.2", RTLD_LAZY);
    if (!h) return -1;
    alsa.handle = h;
    alsa.snd_seq_open = dlsym(h, "snd_seq_open");
    alsa.snd_seq_close = dlsym(h, "snd_seq_close");
    alsa.snd_seq_set_client_name = dlsym(h, "snd_seq_set_client_name");
    alsa.snd_seq_client_id = dlsym(h, "snd_seq_client_id");
    alsa.snd_seq_create_simple_port = dlsym(h, "snd_seq_create_simple_port");
    alsa.snd_seq_connect_to = dlsym(h, "snd_seq_connect_to");
    alsa.snd_seq_connect_from = dlsym(h, "snd_seq_connect_from");
    alsa.snd_seq_client_info_sizeof = dlsym(h, "snd_seq_client_info_sizeof");
    alsa.snd_seq_client_info_set_client = dlsym(h, "snd_seq_client_info_set_client");
    alsa.snd_seq_client_info_get_client = dlsym(h, "snd_seq_client_info_get_client");
    alsa.snd_seq_client_info_get_name = dlsym(h, "snd_seq_client_info_get_name");
    alsa.snd_seq_query_next_client = dlsym(h, "snd_seq_query_next_client");
    alsa.snd_seq_port_info_sizeof = dlsym(h, "snd_seq_port_info_sizeof");
    alsa.snd_seq_port_info_set_client = dlsym(h, "snd_seq_port_info_set_client");
    alsa.snd_seq_port_info_set_port = dlsym(h, "snd_seq_port_info_set_port");
    alsa.snd_seq_port_info_get_port = dlsym(h, "snd_seq_port_info_get_port");
    alsa.snd_seq_port_info_get_name = dlsym(h, "snd_seq_port_info_get_name");
    alsa.snd_seq_port_info_get_capability = dlsym(h, "snd_seq_port_info_get_capability");
    alsa.snd_seq_query_next_port = dlsym(h, "snd_seq_query_next_port");
    alsa.snd_seq_poll_descriptors_count = dlsym(h, "snd_seq_poll_descriptors_count");
    alsa.snd_seq_poll_descriptors = dlsym(h, "snd_seq_poll_descriptors");
    alsa.snd_seq_event_input = dlsym(h, "snd_seq_event_input");
    alsa.snd_seq_event_input_pending = dlsym(h, "snd_seq_event_input_pending");
    alsa.snd_seq_event_output_direct = dlsym(h, "snd_seq_event_output_direct");

    alsa.snd_pcm_open = dlsym(h, "snd_pcm_open");
    alsa.snd_pcm_set_params = dlsym(h, "snd_pcm_set_params");
    alsa.snd_pcm_writei = dlsym(h, "snd_pcm_writei");
    alsa.snd_pcm_recover = dlsym(h, "snd_pcm_recover");
    alsa.snd_pcm_drop = dlsym(h, "snd_pcm_drop");
    alsa.snd_pcm_close = dlsym(h, "snd_pcm_close");
    return 0;
}

int c_open_seq(void **seqp, const char *name) {
    if (!alsa.snd_seq_open) return -1;
    if (alsa.snd_seq_open(seqp, "default", 3, 1) != 0) return -1;
    alsa.snd_seq_set_client_name(*seqp, name);
    return alsa.snd_seq_client_id(*seqp);
}

void c_close_seq(void *seq) {
    if (alsa.snd_seq_close && seq) {
        alsa.snd_seq_close(seq);
    }
}

int c_create_port(void *seq, const char *name, unsigned int caps) {
    if (!alsa.snd_seq_create_simple_port) return -1;
    return alsa.snd_seq_create_simple_port(seq, name, caps, (1 << 20));
}

int c_connect(void *seq, int out_port, int in_port, int dest_client, int dest_port) {
    if (!alsa.snd_seq_connect_to || !alsa.snd_seq_connect_from) return -1;
    alsa.snd_seq_connect_to(seq, out_port, dest_client, dest_port);
    alsa.snd_seq_connect_from(seq, in_port, dest_client, dest_port);
    return 0;
}

int c_send_realtime(void *seq, int out_port, int dest_client, int dest_port, int ev_type) {
    if (!alsa.snd_seq_event_output_direct) return -1;
    snd_seq_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = (uint8_t)ev_type;
    ev.queue = 253; // SND_SEQ_QUEUE_DIRECT
    ev.source.port = (uint8_t)out_port;
    ev.dest.client = (uint8_t)dest_client;
    ev.dest.port = (uint8_t)dest_port;
    return alsa.snd_seq_event_output_direct(seq, &ev);
}

int c_poll_wait(void *seq, int timeout_ms) {
    if (!alsa.snd_seq_poll_descriptors_count || !alsa.snd_seq_poll_descriptors) return 0;
    int count = alsa.snd_seq_poll_descriptors_count(seq, POLLIN);
    if (count <= 0) return 0;
    struct pollfd pfds[8];
    if (count > 8) count = 8;
    alsa.snd_seq_poll_descriptors(seq, pfds, count, POLLIN);
    return poll(pfds, count, timeout_ms);
}

int c_event_pending(void *seq) {
    if (alsa.snd_seq_event_input_pending && seq) {
        return alsa.snd_seq_event_input_pending(seq, 0);
    }
    return 0;
}

int c_poll_event(void *seq, int *ev_type, int *ch, int *note, int *vel) {
    if (!alsa.snd_seq_event_input) return 0;
    snd_seq_event_t *ev = NULL;
    int res = alsa.snd_seq_event_input(seq, &ev);
    if (res >= 0 && ev != NULL) {
        *ev_type = ev->type;
        if (ev->type == 6 || ev->type == 7) {
            *ch = ev->data.note.channel;
            *note = ev->data.note.note;
            *vel = ev->data.note.velocity;
        }
        return 1;
    }
    return 0;
}

int c_find_target(void *seq, int my_client_id, const char *pattern, int *out_client, int *out_port, char *out_name, int out_name_len) {
    if (!alsa.snd_seq_client_info_sizeof || !alsa.snd_seq_port_info_sizeof) return 0;
    void *cinfo = malloc(alsa.snd_seq_client_info_sizeof());
    void *pinfo = malloc(alsa.snd_seq_port_info_sizeof());
    if (!cinfo || !pinfo) {
        if (cinfo) free(cinfo);
        if (pinfo) free(pinfo);
        return 0;
    }

    alsa.snd_seq_client_info_set_client(cinfo, -1);
    int found = 0;

    while (alsa.snd_seq_query_next_client(seq, cinfo) >= 0) {
        int cid = alsa.snd_seq_client_info_get_client(cinfo);
        if (cid == my_client_id || cid == 0) continue;

        const char *cname = alsa.snd_seq_client_info_get_name(cinfo);
        if (!cname) continue;

        int match = 0;
        if (strcasestr(cname, pattern) != NULL) {
            match = 1;
        } else if (strcmp(pattern, "all") == 0 && strcasestr(cname, "pipewire") == NULL) {
            match = 1;
        }

        if (match) {
            alsa.snd_seq_port_info_set_client(pinfo, cid);
            alsa.snd_seq_port_info_set_port(pinfo, -1);
            while (alsa.snd_seq_query_next_port(seq, pinfo) >= 0) {
                int pid = alsa.snd_seq_port_info_get_port(pinfo);
                const char *pname = alsa.snd_seq_port_info_get_name(pinfo);
                *out_client = cid;
                *out_port = pid;
                snprintf(out_name, out_name_len, "%s (%s)", cname, pname ? pname : "");
                found = 1;
                break;
            }
        }
        if (found) break;
    }

    free(cinfo);
    free(pinfo);
    return found;
}

// PCM C wrappers
int c_pcm_open(void **pcm, const char *name, unsigned int rate, unsigned int latency_us) {
    if (!alsa.snd_pcm_open || !alsa.snd_pcm_set_params) return -1;
    if (alsa.snd_pcm_open(pcm, name, 0, 0) != 0) return -1;
    // format 2 = S16_LE, access 3 = RW_INTERLEAVED, channels 2
    if (alsa.snd_pcm_set_params(*pcm, 2, 3, 2, rate, 1, latency_us) != 0) {
        if (alsa.snd_pcm_close) alsa.snd_pcm_close(*pcm);
        *pcm = NULL;
        return -2;
    }
    return 0;
}

long c_pcm_write(void *pcm, const void *buf, unsigned long frames) {
    if (!pcm || !alsa.snd_pcm_writei) return -1;
    long written = alsa.snd_pcm_writei(pcm, buf, frames);
    if (written < 0 && alsa.snd_pcm_recover) {
        int err = alsa.snd_pcm_recover(pcm, (int)written, 0);
        if (err == 0 && alsa.snd_pcm_writei) {
            written = alsa.snd_pcm_writei(pcm, buf, frames);
        }
    }
    return written;
}

void c_pcm_close(void *pcm) {
    if (pcm && alsa.snd_pcm_close) {
        if (alsa.snd_pcm_drop) alsa.snd_pcm_drop(pcm);
        alsa.snd_pcm_close(pcm);
    }
}
