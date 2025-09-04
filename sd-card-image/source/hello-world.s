  .org $ff00    ; Though written to the first address in the ROM, $0,
                ; this code will appear to the CPU to be at $8000

CHOUT=$f001

reset:          ; This label marks the first position in the ROM for
                ; the CPU is $8000
  ldy #$00
loop:
  lda text,y
  beq done
  sta CHOUT
  iny
  jmp loop
  lda #$50
  sta $6000
done:
    lda #$0a
    sta CHOUT
spin:
    jmp spin

text:
    .asciiz "Hello World!"

    .org $fffc   ; Specify that the following code will go at the position
               ; that appears to the CPU to be at $fffc. $fffc is the
               ; location of the reset vector. The value at this vector
               ; is loaded into the program counter after a CPU reset.

    .word reset  ; Place the value of the label reset, ie $8000, at this
               ; position

    .word $0000  ; Pad out the last couple bytes

