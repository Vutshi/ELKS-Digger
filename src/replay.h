/* ELKS Digger deterministic replay support.
 * Optional; compiled only when DIGGER_REPLAY is defined.
 */
#ifndef DIGGER_REPLAY_H
#define DIGGER_REPLAY_H

#ifdef DIGGER_REPLAY

#define REPLAY_MASK_P1_RIGHT 0x0001u
#define REPLAY_MASK_P1_UP    0x0002u
#define REPLAY_MASK_P1_LEFT  0x0004u
#define REPLAY_MASK_P1_DOWN  0x0008u
#define REPLAY_MASK_P1_FIRE  0x0010u
#define REPLAY_MASK_P2_RIGHT 0x0020u
#define REPLAY_MASK_P2_UP    0x0040u
#define REPLAY_MASK_P2_LEFT  0x0080u
#define REPLAY_MASK_P2_DOWN  0x0100u
#define REPLAY_MASK_P2_FIRE  0x0200u
#define REPLAY_MASK_ESCAPE   0x0400u
#define REPLAY_MASK_SPEEDUP  0x0800u
#define REPLAY_MASK_SPEEDDN  0x1000u
#define REPLAY_MASK_PAUSE    0x2000u

void replay_set_record_name(char *name);
void replay_set_play_name(char *name);
void replay_set_fast(void);
int replay_is_recording(void);
int replay_is_playing(void);
int replay_fast(void);
int replay_has_request(void);
int replay_exit_status(void);
void replay_record_header(void);
int replay_play_header(void);
void replay_begin_block(Uint5 seed);
Uint5 replay_play_seed(void);
void replay_end_block(void);
void replay_finish(void);
void replay_frame_boundary(void);
void replay_note_event(unsigned int mask);
unsigned int replay_capture_input(void);
void replay_inject_input(unsigned int mask);

#else

#define replay_set_record_name(name)     ((void)(name))
#define replay_set_play_name(name)       ((void)(name))
#define replay_set_fast()                ((void)0)
#define replay_is_recording()            (0)
#define replay_is_playing()              (0)
#define replay_fast()                    (0)
#define replay_has_request()             (0)
#define replay_exit_status()             (0)
#define replay_record_header()           ((void)0)
#define replay_play_header()             (0)
#define replay_begin_block(seed)         ((void)(seed))
#define replay_play_seed()               ((Uint5)0)
#define replay_end_block()               ((void)0)
#define replay_finish()                  ((void)0)
#define replay_frame_boundary()          ((void)0)
#define replay_note_event(mask)          ((void)(mask))

#endif

#endif /* DIGGER_REPLAY_H */
