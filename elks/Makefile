# ELKS Digger port build file.
#
# Copyright (C) 2026 Denis Vasilyev <Vutshi>
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Minimal Digger Makefile for ELKS/gcc-ia16, CGA-only with optional PC speaker sound.

############# Standard Section for IA16 C ##############
ifeq ($(filter regen-assets clean,$(MAKECMDGOALS)),)
ifndef TOPDIR
$(error ELKS TOPDIR is not defined)
endif
endif

CC = ia16-elf-gcc
LD = ia16-elf-gcc
HOSTCC ?= cc
HOSTCFLAGS ?= -O2 -I../src
CLBASE = -melks-libc -mtune=i8086 -mcmodel=small -mno-segment-relocation-stuff
CLBASE += -fno-inline -fno-builtin-printf -fno-builtin-fprintf
OPTFLAGS = -Os
OPTFLAGS += -fno-align-jumps
OPTFLAGS += -fno-align-functions
OPTFLAGS += -fno-align-loops
OPTFLAGS += -fno-align-labels
OPTFLAGS += -fira-region=one
OPTFLAGS += -fira-hoist-pressure
OPTFLAGS += -freorder-blocks-algorithm=simple
OPTFLAGS += -fno-reorder-blocks
OPTFLAGS += -fno-prefetch-loop-arrays
OPTFLAGS += -fno-tree-ch
WARNINGS = -Wall -Wextra -Wtype-limits -Wno-unused-parameter -Wno-sign-compare
INCLUDES = -I. -I../src -Igenerated -I$(TOPDIR)/include -I$(TOPDIR)/libc/include -I$(TOPDIR)/elks/include
DEFINES = -include config_elks.h
CFLAGS = $(OPTFLAGS) $(CLBASE) $(WARNINGS) $(INCLUDES) $(DEFINES) $(LOCALFLAGS)
LDFLAGS = $(CLBASE)
LDLIBS =

%.oaj: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.oaj: %.S
	$(CC) $(CLBASE) -x assembler-with-cpp -c -o $@ $<

%.oaj: ../src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

generated/%.oaj: generated/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

############# End of Standard Section ##############

BINDIR = .
PROG = $(BINDIR)/digger
LOCALFLAGS = -DDIGGER_ELKS -DDIGGER_CGA_ONLY
# SOUND=dos uses the original DOS Digger sound state machine with
# the ELKS direct PC speaker backend.
# SOUND=direct keeps the older small ELKS approximation.
# SOUND=0 keeps the silent/stub build.
# SOUND=seq uses the ELKS kernel PC-speaker event sequencer for background music.
SOUND ?= seq
# Keyboard backend:
#   KEYBOARD=kraw  default, ELKS console raw scan-code make/break input
#   KEYBOARD=term  compatibility, old terminal-byte/ANSI escape input
KEYBOARD ?= kraw
PROFILE ?= 0
SPEED ?= 80000
TITLEBMP ?= 1
LOCALFLAGS += -DDIGGER_ELKS_DEFAULT_FTIME=$(SPEED)
ifeq ($(KEYBOARD),kraw)
LOCALFLAGS += -DDIGGER_ELKS_KBD_KRAW
else ifeq ($(KEYBOARD),term)
LOCALFLAGS += -DDIGGER_ELKS_KBD_TERM
else
$(error Unknown KEYBOARD=$(KEYBOARD), expected kraw or term)
endif
ifeq ($(TITLEBMP),1)
LOCALFLAGS += -DDIGGER_ELKS_TITLE_BMP
TITLEBMP_ASSET = $(BINDIR)/digtitle.bmp
endif
ifeq ($(PROFILE),1)
LOCALFLAGS += -DDIGGER_CGA_PROFILE
endif
ifeq ($(SOUND),0)
LOCALFLAGS += -DDIGGER_SOUND_STUB
SOUND_SRC = elks_sound_stub.c
else ifeq ($(SOUND),direct)
LOCALFLAGS += -DDIGGER_ELKS_PCSPK_DIRECT
SOUND_SRC = elks_sound_stub.c
else ifeq ($(SOUND),seq)
LOCALFLAGS += -DDIGGER_ELKS_PCSPK_SEQ
SOUND_SRC = elks_sound_seq.c
else
LOCALFLAGS += -DDIGGER_ELKS_PCSPK_DIRECT -DDIGGER_ELKS_DOS_SOUND
SOUND_SRC = elks_sound_dos.c
endif

CORE_SRCS = \
	../src/main.c ../src/digger.c ../src/monster.c ../src/bags.c ../src/drawing.c ../src/sprite.c \
	../src/scores.c ../src/input.c

PORT_SRCS = \
	elks_hw.c elks_input.c elks_cga.c $(SOUND_SRC)

# CGA-only asset payload.  Do not add generated/title_cga.c until the
# executable-size/near-data budget has been measured on the target loader.
ASSET_SRCS = \
	generated/cgagrafx.c generated/alpha_cga.c

ASM_OBJS = elks_cga_asm.oaj

OBJS = $(CORE_SRCS:../src/%.c=%.oaj) \
       $(PORT_SRCS:.c=.oaj) \
       $(ASSET_SRCS:.c=.oaj) \
       $(ASM_OBJS)

all: $(PROG) $(TITLEBMP_ASSET)

MKG_HOST = mkg.host

$(MKG_HOST): ../src/mkg.c ../src/def.h
	$(HOSTCC) $(HOSTCFLAGS) -o $@ ../src/mkg.c

regen-assets: $(MKG_HOST)
	cd ../src && elks/$(MKG_HOST) elks/generated/cgagrafx.c
	cd ../src && elks/$(MKG_HOST) elks/generated/alpha_cga.c

$(PROG): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BINDIR)/digtitle.bmp: ../src/ctitle.bmp
	cp $< $@

SOUNDTEST_OBJS = pcspktest.oaj

soundtest: pcspktest

pcspktest: $(SOUNDTEST_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(PROG) pcspktest $(MKG_HOST) *.oaj generated/*.oaj $(BINDIR)/digtitle.bmp

.PHONY: all regen-assets soundtest clean
