; license:MIT License
; copyright-holders:Hiromasa Tanaka

; rodata_user
; https://github.com/z88dk/z88dk/blob/master/doc/overview.md#a-quick-note-for-asm-code
; rodata_user if for constant data
; kept in rom if program is in rom
SECTION rodata_user
PUBLIC _pcg_char, _pcg_color
PUBLIC _sprite_char1_color1, _sprite_char1_color2, _sprite_char2_color1, _sprite_char2_color2

include "../resources/graphic_pcg.asm"
include "../resources/graphic_sprite.asm"
