; license:MIT License
; copyright-holders:Hiromasa Tanaka

; rodata_user
; https://github.com/z88dk/z88dk/blob/master/doc/overview.md#a-quick-note-for-asm-code
; rodata_user if for constant data
; kept in rom if program is in rom
SECTION rodata_user
PUBLIC _music_title, _music_main, _sound_extend, _music_game_over, _music_game_over_2

include "../resources/music_title.asm"
include "../resources/music_main.asm"
include "../resources/music_game_over.asm"
include "../resources/music_game_over_2.asm"
include "../resources/sound_extend.asm"
