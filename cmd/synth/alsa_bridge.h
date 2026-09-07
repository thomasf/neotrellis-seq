#ifndef ALSA_BRIDGE_H
#define ALSA_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// ALSA Sequencer Types & Constants
// ============================================================================
typedef struct {
    uint8_t client;
    uint8_t port;
} snd_seq_addr_t;

typedef struct {
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t off_velocity;
    uint32_t duration;
} snd_seq_ev_note_t;

typedef union {
    uint32_t tick;
    uint32_t time[2];
} snd_seq_timestamp_t;

typedef union {
    snd_seq_ev_note_t note;
    snd_seq_addr_t addr;
    uint8_t raw[12];
} snd_seq_event_data_t;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint8_t tag;
    uint8_t queue;
    snd_seq_timestamp_t time;
    snd_seq_addr_t source;
    snd_seq_addr_t dest;
    snd_seq_event_data_t data;
} snd_seq_event_t;

// ============================================================================
// Function Declarations
// ============================================================================
int init_alsa(void);

int c_open_seq(void **seqp, const char *name);
void c_close_seq(void *seq);

int c_create_port(void *seq, const char *name, unsigned int caps);
int c_connect(void *seq, int out_port, int in_port, int dest_client, int dest_port);

int c_send_realtime(void *seq, int out_port, int dest_client, int dest_port, int ev_type);

int c_poll_wait(void *seq, int timeout_ms);
int c_event_pending(void *seq);
int c_poll_event(void *seq, int *ev_type, int *ch, int *note, int *vel);

int c_find_target(void *seq, int my_client_id, const char *pattern, int *out_client, int *out_port, char *out_name, int out_name_len);

// PCM API
int c_pcm_open(void **pcm, const char *name, unsigned int rate, unsigned int latency_us);
long c_pcm_write(void *pcm, const void *buf, unsigned long frames);
void c_pcm_close(void *pcm);

#ifdef __cplusplus
}
#endif

#endif // ALSA_BRIDGE_H
