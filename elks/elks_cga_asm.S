/*
 * ELKS CGA blitter hot loops for Digger.
 *
 * Based on / adapted from the original Digger DOS CGA assembly code.
 *
 * Original Digger code:
 *   Copyright (C) Andrew Jenner 1998-2004
 *
 * ELKS port modifications:
 *   Copyright (C) 2026 Denis Vasilyev <Vutshi>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* ELKS Digger CGA hot loops for ia16-elf-gcc. */
        .code16
        .text
#define CGA_SEGMENT     0xB800
#define CGA_BYTES       0x4000
#define CGA_NEXT_FIELD  0x2000
#define CGA_WRAP_DELTA  0x3FB0
#define arg1            4

        .global elks_cga_clear_asm
elks_cga_clear_asm:
        push    %di
        push    %es
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        xor     %di, %di
        xor     %ax, %ax
        mov     $8192, %cx              /* CGA_BYTES / 2 words */
        cld
        rep     stosw
        pop     %es
        pop     %di
        ret

#define title_off       arg1
#define title_src       arg1 + 2
        .global elks_cga_title_scanline_asm
elks_cga_title_scanline_asm:
        push    %bp
        mov     %sp, %bp
        push    %si
        push    %di
        push    %es
        mov     title_off(%bp), %di
        mov     title_src(%bp), %si
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        mov     $40, %cx                /* 80 bytes / 2 words */
        cld
        rep     movsw
        pop     %es
        pop     %di
        pop     %si
        pop     %bp
        ret

#define puti2_off       arg1
#define puti2_src       arg1 + 2
#define puti2_h         arg1 + 4
        .global elks_cga_puti2_asm
elks_cga_puti2_asm:
        push    %bp
        mov     %sp, %bp
        push    %si
        push    %di
        push    %es
        mov     puti2_off(%bp), %di
        mov     puti2_src(%bp), %si
        mov     puti2_h(%bp), %cx
        jcxz    Lputi2_done
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        cld
Lputi2_row:
        movsw
        sub     $2, %di
        add     $CGA_NEXT_FIELD, %di
        cmp     $CGA_BYTES, %di
        jb      Lputi2_next
        sub     $CGA_WRAP_DELTA, %di
Lputi2_next:
        loop    Lputi2_row
Lputi2_done:
        pop     %es
        pop     %di
        pop     %si
        pop     %bp
        ret

#define puti4_off       arg1
#define puti4_src       arg1 + 2
#define puti4_h         arg1 + 4
        .global elks_cga_puti4_asm
elks_cga_puti4_asm:
        push    %bp
        mov     %sp, %bp
        push    %si
        push    %di
        push    %es
        mov     puti4_off(%bp), %di
        mov     puti4_src(%bp), %si
        mov     puti4_h(%bp), %cx
        jcxz    Lputi4_done
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        cld
Lputi4_row:
        movsw
        movsw
        sub     $4, %di
        add     $CGA_NEXT_FIELD, %di
        cmp     $CGA_BYTES, %di
        jb      Lputi4_next
        sub     $CGA_WRAP_DELTA, %di
Lputi4_next:
        loop    Lputi4_row
Lputi4_done:
        pop     %es
        pop     %di
        pop     %si
        pop     %bp
        ret

#define geti2_off       arg1
#define geti2_dst       arg1 + 2
#define geti2_h         arg1 + 4
        .global elks_cga_geti2_asm
elks_cga_geti2_asm:
        push    %bp
        mov     %sp, %bp
        push    %si
        push    %di
        push    %ds
        push    %es
        mov     geti2_off(%bp), %si
        mov     geti2_dst(%bp), %di
        mov     geti2_h(%bp), %cx
        jcxz    Lgeti2_done
        push    %ds
        pop     %es
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %ds
        cld
Lgeti2_row:
        movsw
        sub     $2, %si
        add     $CGA_NEXT_FIELD, %si
        cmp     $CGA_BYTES, %si
        jb      Lgeti2_next
        sub     $CGA_WRAP_DELTA, %si
Lgeti2_next:
        loop    Lgeti2_row
Lgeti2_done:
        pop     %es
        pop     %ds
        pop     %di
        pop     %si
        pop     %bp
        ret

#define geti4_off       arg1
#define geti4_dst       arg1 + 2
#define geti4_h         arg1 + 4
        .global elks_cga_geti4_asm
elks_cga_geti4_asm:
        push    %bp
        mov     %sp, %bp
        push    %si
        push    %di
        push    %ds
        push    %es
        mov     geti4_off(%bp), %si
        mov     geti4_dst(%bp), %di
        mov     geti4_h(%bp), %cx
        jcxz    Lgeti4_done
        push    %ds
        pop     %es
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %ds
        cld
Lgeti4_row:
        movsw
        movsw
        sub     $4, %si
        add     $CGA_NEXT_FIELD, %si
        cmp     $CGA_BYTES, %si
        jb      Lgeti4_next
        sub     $CGA_WRAP_DELTA, %si
Lgeti4_next:
        loop    Lgeti4_row
Lgeti4_done:
        pop     %es
        pop     %ds
        pop     %di
        pop     %si
        pop     %bp
        ret

#define putim4_off      arg1
#define putim4_dat      arg1 + 2
#define putim4_msk      arg1 + 4
#define putim4_h        arg1 + 6
        .global elks_cga_putim4_asm
elks_cga_putim4_asm:
        push    %bp
        mov     %sp, %bp
        push    %bx
        push    %si
        push    %di
        push    %es
        mov     putim4_off(%bp), %di
        mov     putim4_dat(%bp), %si
        mov     putim4_msk(%bp), %bx
        mov     putim4_h(%bp), %cx
        jcxz    Lputim4_done
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        cld
Lputim4_row:
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        sub     $4, %di
        add     $CGA_NEXT_FIELD, %di
        cmp     $CGA_BYTES, %di
        jb      Lputim4_next
        sub     $CGA_WRAP_DELTA, %di
Lputim4_next:
        loop    Lputim4_row
Lputim4_done:
        pop     %es
        pop     %di
        pop     %si
        pop     %bx
        pop     %bp
        ret

#define putim2_off      arg1
#define putim2_dat      arg1 + 2
#define putim2_msk      arg1 + 4
#define putim2_h        arg1 + 6
        .global elks_cga_putim2_asm
elks_cga_putim2_asm:
        push    %bp
        mov     %sp, %bp
        push    %bx
        push    %si
        push    %di
        push    %es
        mov     putim2_off(%bp), %di
        mov     putim2_dat(%bp), %si
        mov     putim2_msk(%bp), %bx
        mov     putim2_h(%bp), %cx
        jcxz    Lputim2_done
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        cld
Lputim2_row:
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        sub     $2, %di
        add     $CGA_NEXT_FIELD, %di
        cmp     $CGA_BYTES, %di
        jb      Lputim2_next
        sub     $CGA_WRAP_DELTA, %di
Lputim2_next:
        loop    Lputim2_row
Lputim2_done:
        pop     %es
        pop     %di
        pop     %si
        pop     %bx
        pop     %bp
        ret

#define putim5_off      arg1
#define putim5_dat      arg1 + 2
#define putim5_msk      arg1 + 4
#define putim5_h        arg1 + 6
        .global elks_cga_putim5_asm
elks_cga_putim5_asm:
        push    %bp
        mov     %sp, %bp
        push    %bx
        push    %si
        push    %di
        push    %es
        mov     putim5_off(%bp), %di
        mov     putim5_dat(%bp), %si
        mov     putim5_msk(%bp), %bx
        mov     putim5_h(%bp), %cx
        jcxz    Lputim5_done
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        cld
Lputim5_row:
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        sub     $5, %di
        add     $CGA_NEXT_FIELD, %di
        cmp     $CGA_BYTES, %di
        jb      Lputim5_next
        sub     $CGA_WRAP_DELTA, %di
Lputim5_next:
        loop    Lputim5_row
Lputim5_done:
        pop     %es
        pop     %di
        pop     %si
        pop     %bx
        pop     %bp
        ret

#define putim6_off      arg1
#define putim6_dat      arg1 + 2
#define putim6_msk      arg1 + 4
#define putim6_h        arg1 + 6
        .global elks_cga_putim6_asm
elks_cga_putim6_asm:
        push    %bp
        mov     %sp, %bp
        push    %bx
        push    %si
        push    %di
        push    %es
        mov     putim6_off(%bp), %di
        mov     putim6_dat(%bp), %si
        mov     putim6_msk(%bp), %bx
        mov     putim6_h(%bp), %cx
        jcxz    Lputim6_done
        mov     $CGA_SEGMENT, %ax
        mov     %ax, %es
        cld
Lputim6_row:
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        movb    (%bx), %al
        andb    %al, %es:(%di)
        lodsb
        orb     %al, %es:(%di)
        inc     %bx
        inc     %di
        sub     $6, %di
        add     $CGA_NEXT_FIELD, %di
        cmp     $CGA_BYTES, %di
        jb      Lputim6_next
        sub     $CGA_WRAP_DELTA, %di
Lputim6_next:
        loop    Lputim6_row
Lputim6_done:
        pop     %es
        pop     %di
        pop     %si
        pop     %bx
        pop     %bp
        ret
