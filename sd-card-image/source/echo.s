  .org $ff00    ; Though written to the first address in the ROM, $0,
                ; this code will appear to the CPU to be at $8000

;CHOUT=$f001
CIN             = $f004
COUT            = $f001

reset:          ; This label marks the first position in the ROM for
                ; the CPU is $8000
getkey:
    jsr CHRIN
    cmp #$00
    beq getkey
;    lda $f004
;    beq getkey
;    jsr CHROUT
    cmp #$0d
    bne getkey
    lda #$0a
    jsr CHROUT
;    sta $f001
    jmp getkey

; Input a character from the serial interface.
; On return, carry flag indicates whether a key was pressed
; If a key was pressed, the key value will be in the A register
;
; Modifies: flags, A
MONRDKEY:
CHRIN:
                phx
                LDA CIN
;                jsr     BUFFER_SIZE
                beq     no_keypressed
;               jsr     READ_BUFFER
                jsr     CHROUT                  ; echo
                pha
;                jsr     BUFFER_SIZE
;                cmp     #$B0
;                bcs     @mostly_full
;                lda     #$fe
;                and     PORTA
;                sta     PORTA
mostly_full:
                pla
                plx
 ;               sec
                rts
no_keypressed:
                plx
;                clc
                rts


; Output a character (from the A register) to the serial interface.
;
; Modifies: flags
MONCOUT:
CHROUT:
                pha
                sta COUT
;                sta     ACIA_DATA
;                lda     #$FF
;@txdelay:       dec
;                bne     @txdelay
                pla
                rts

; Initialize the circular input buffer
; Modifies: flags, A
INIT_BUFFER:
;                lda READ_PTR
;                sta WRITE_PTR
;                lda #$01
;                sta DDRA
;                lda #$fe
;                and PORTA
;                sta PORTA
                rts

; Write a character (from the A register) to the circular input buffer
; Modifies: flags, X
WRITE_BUFFER:
;                ldx WRITE_PTR
;                sta INPUT_BUFFER,x
;                inc WRITE_PTR
                rts

; Read a character from the circular input buffer and put it in the A register
; Modifies: flags, A, X
READ_BUFFER:
;                ldx READ_PTR
;                lda INPUT_BUFFER,x
;                inc READ_PTR
                rts

; Return (in A) the number of unread bytes in the circular input buffer
; Modifies: flags, A
BUFFER_SIZE:
;                lda WRITE_PTR
;                sec
;                sbc READ_PTR
                rts


; Interrupt request handler
IRQ_HANDLER:
;                pha
;                phx
;                lda     ACIA_STATUS
;                ; For now, assume the only source of interrupts is incoming data
;                lda     ACIA_DATA
;                jsr     WRITE_BUFFER
;                jsr     BUFFER_SIZE
;                cmp     #$F0
;                bcc     @not_full
;                lda     #$01
;                ora     PORTA
;                sta     PORTA
;@not_full:
;                plx
;                pla
                rti

    .org $fffc   ; Specify that the following code will go at the position
               ; that appears to the CPU to be at $fffc. $fffc is the
               ; location of the reset vector. The value at this vector
               ; is loaded into the program counter after a CPU reset.

    .word reset  ; Place the value of the label reset, ie $8000, at this
               ; position

    .word $0000  ; Pad out the last couple bytes
