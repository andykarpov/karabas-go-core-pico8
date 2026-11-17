/* the cart under test, path from the Makefile (-DCART_FILE="...") */
    .section .rodata.cart
    .global cart_data
    .global cart_end
    .balign 4
cart_data:
    .incbin CART_FILE
cart_end:
    .byte 0
