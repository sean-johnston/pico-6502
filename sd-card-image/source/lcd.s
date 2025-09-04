  .ORG $FE00    ; THOUGH WRITTEN TO THE FIRST ADDRESS IN THE ROM, $0,
                ; THIS CODE WILL APPEAR TO THE CPU TO BE AT $FE000

CHROUT=$F001
CHRIN=$F004
PORTB = $D000
DDRB = $D002
E  = %01000000
RW = %00100000
RS = %00010000

RESET:                      ; Reset vector
            JSR LCDINIT     ; Initialize the LCD
            LDY #$00        ; Set message index to 0
NEXT_CHAR:
            LDA MESSAGE,Y   ; Load accumulator from message indexed by y
            CMP #$00        ; Check if we reached the zero in the string
            BEQ LOOP        ; If we did, jump to the loop
            INY             ; Increment the index
            JSR LCD_PRINT_CHAR ; Print character
            JMP NEXT_CHAR   ; Do the next caracter
LOOP:
            JMP LOOP        ; Spin in the loop
MESSAGE:
    .ASCIIZ "HELLO WORLD!"

LCD_WAIT:
            PHA
            LDA #'A'
            STA CHROUT
            LDA #%11110000  ; LCD data is input
            STA DDRB        ; Store in B data direction register 
LCDBUSY:
            LDA #'B'
            STA CHROUT
            LDA #RW         ; Turn on the RW pin
            STA PORTB
            LDA #(RW | E)   ; Turn on the E pin with the RW pin
            STA PORTB
            LDA PORTB       ; Read the high nibble
            PHA             ; and put on the stack since it has the busy flag
            LDA #RW         ; Turn on the RW pin
            STA PORTB
            LDA #(RW | E)   ; Turn on the E pin with the RW pin
            STA PORTB
            LDA PORTB       ; Read the low nibble
            PLA             ; Get the high nibble off the stack
            AND #%00001000  ; Check the busy bit
            BNE LCDBUSY     ; If busy continue

            LDA #RW
            STA PORTB
            LDA #%11111111  ; LCD DATA IS OUTPUT
            STA DDRB
            LDA #'C'
            STA CHROUT
            PLA
            RTS

LCDINIT:
            LDA #$FF        ; SET ALL PINS ON PORT B TO OUTPUT
            STA DDRB

            LDA #%00000011  ; SET 8-BIT MODE
            STA PORTB
            ORA #E
            STA PORTB
            AND #%00001111
            STA PORTB

            LDA #%00000011  ; SET 8-BIT MODE
            STA PORTB
            ORA #E
            STA PORTB
            AND #%00001111
            STA PORTB

            LDA #%00000011  ; SET 8-BIT MODE
            STA PORTB
            ORA #E
            STA PORTB
            AND #%00001111
            STA PORTB

            ; OKAY, NOW WE'RE REALLY IN 8-BIT MODE.
            ; COMMAND TO GET TO 4-BIT MODE OUGHT TO WORK NOW
            LDA #%00000010  ; SET 4-BIT MODE
            STA PORTB
            ORA #E
            STA PORTB
            AND #%00001111
            STA PORTB

            LDA #%00101000  ; SET 4-BIT MODE; 2-LINE DISPLAY; 5X8 FONT
            JSR LCD_INSTRUCTION
            LDA #%00001110  ; DISPLAY ON; CURSOR ON; BLINK OFF
            JSR LCD_INSTRUCTION
            LDA #%00000110  ; INCREMENT AND SHIFT CURSOR; DON'T SHIFT DISPLAY
            JSR LCD_INSTRUCTION
            LDA #%00000001  ; CLEAR DISPLAY
            JSR LCD_INSTRUCTION
            RTS

LCD_INSTRUCTION:
            JSR LCD_WAIT
            PHA
            LSR
            LSR
            LSR
            LSR             ; SEND HIGH 4 BITS
            STA PORTB
            ORA #E          ; SET E BIT TO SEND INSTRUCTION
            STA PORTB
            EOR #E          ; CLEAR E BIT
            STA PORTB
            PLA
            AND #%00001111  ; SEND LOW 4 BITS
            STA PORTB
            ORA #E          ; SET E BIT TO SEND INSTRUCTION
            STA PORTB
            EOR #E          ; CLEAR E BIT
            STA PORTB
            RTS

LCD_PRINT_CHAR:
            JSR LCD_WAIT    ; Jump to the LCD_WAIT subroutine
            PHA             ; Push accumulator on the stack
            LSR             ; Move high nibble to low nibble
            LSR
            LSR
            LSR             ; SEND HIGH 4 BITS
            ORA #RS         ; SET RS
            STA PORTB
            ORA #E          ; SET E BIT TO SEND INSTRUCTION
            STA PORTB
            EOR #E          ; CLEAR E BIT
            STA PORTB
            PLA
            AND #%00001111  ; SEND LOW 4 BITS
            ORA #RS         ; SET RS
            STA PORTB
            ORA #E          ; SET E BIT TO SEND INSTRUCTION
            STA PORTB
            EOR #E          ; CLEAR E BIT
            STA PORTB
            RTS

    .ORG $FFFC   ; SPECIFY THAT THE FOLLOWING CODE WILL GO AT THE POSITION
               ; THAT APPEARS TO THE CPU TO BE AT $FFFC. $FFFC IS THE
               ; LOCATION OF THE RESET VECTOR. THE VALUE AT THIS VECTOR
               ; IS LOADED INTO THE PROGRAM COUNTER AFTER A CPU RESET.

    .WORD RESET  ; PLACE THE VALUE OF THE LABEL RESET, IE $8000, AT THIS
               ; POSITION

    .WORD $0000  ; PAD OUT THE LAST COUPLE BYTES

