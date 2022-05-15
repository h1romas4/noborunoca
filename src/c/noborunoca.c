// license:MIT License
// copyright-holders:Hiromasa Tanaka
#include <stdio.h>
#include <stdlib.h>

#include <msx/gfx.h>
#include "psgdriver.h"

#define MSX_CLIKSW  0xf3db
#define MSX_JIFFY   0xfc9e
#define MSX_H_TIMI  0xfd9f

#define VRAM_NONE   0x20
#define VRAM_START  0x1800
#define VRAM_WIDTH  32
#define VRAM_HEIGHT 24
#define VPOS(x, y)  (VRAM_START + VRAM_WIDTH * y + x)
#define HIDDEN_SPRITE_Y 176 + 16

#define STAGE_WIDTH 24
#define STAGE_HEIGHT 24

#define MY_INIT_X 176 - 8
#define MY_INIT_Y 184 - 16
#define MY_INIT_VX -2
#define MY_INIT_VY 0

#define GAME_INIT_PLAYER 2
#define GAME_INIT_LEVEL 1
#define GAME_INIT_POWER 1
#define GAME_INIT_POWER_V 2
#define GAME_INIT_POWER_TRIGGER 0
#define GAME_POWER_MIN 0
#define GAME_POWER_MAX 56
#define GAME_JUMP_EXTENTION_FRAME 4
#define GAME_LEVEL_EXTEND 20
#define GAME_INIT_HISCORE 300
#define GAME_EXTEND_FLOOR 30

#define GAME_SCAFFOLD_FILL 0x3fffff // 22bit

// PCG のラベルの参照(VRAM PCG 転送用)
extern uint8_t pcg_char[], pcg_color[];
// スプライトのラベルの参照(VRAM PCG 転送用)
extern uint8_t sprite_char1_color1[], sprite_char1_color2[];
extern uint8_t sprite_char2_color1[], sprite_char2_color2[];
// 音楽・効果音のラベル参照（psgdriver.asm 用）
extern uint8_t music_title[], music_main[], music_game_over[], music_game_over_2[], sound_extend[];
// 特殊効果音（ジャンプ）
extern uint8_t sound_external_jump[];
// サウンドステータス（ワークエリアから取得）
extern uint8_t sounddrv_bgmwk[];

// ゲームの状態遷移
typedef enum {
    TITLE_INIT,
    TITLE_ADVERTISE,
    GAME_INIT,
    GAME_MAIN,
    GAME_OVER
} game_state_t;

// ゲームの状態
typedef struct {
    game_state_t state;
    uint8_t remein_clear;
    uint16_t score;
    uint16_t score_hi;
    int8_t power;
    int8_t power_v;
    uint8_t power_trigger;
    uint8_t jump_extention_tick;
    uint8_t level;
    uint8_t level_internal;
    uint8_t stick_state;
    uint8_t sound_play;
    uint8_t sound_jump_index;
    uint16_t tick;
    uint8_t trigger_state;
} game_t;

game_t game;

// ステージの状態
typedef struct {
    // STAGE_WIDTH 22bit
    uint32_t scaffold[STAGE_HEIGHT + /* スクロール範囲外 */ 1];
    uint8_t scroll_adjust;
    uint8_t climbed_pos; // time extend
} game_stage_t;

game_stage_t game_stage;

// キャラクターの状態
typedef struct {
    uint8_t sprite_no;
    int16_t x;
    int16_t y;
    int16_t x_prev;
    int16_t y_prev;
    bool can_jump;
    int16_t jump_increment;
    int8_t vx;
    int8_t vy;
    uint16_t tick;
} charctor_t;

// 主人公の状態
charctor_t my;

// タイトルアドバタイズデモの状態
typedef struct {
    uint16_t y;
    int16_t vy;
    uint16_t tick;
    uint8_t trigger_state;
} title_t;

title_t title;

/**
 * vsync カウント待ち
 */
void wait_vsync(uint16_t count)
{
    uint16_t *interval_timer = (uint16_t *)MSX_JIFFY;
    *interval_timer = 0;
    while(*interval_timer < count);
}

/**
 * グラフィックス初期化
 */
void init_graphics()
{
    // スクリーンモード
    set_color(15, 1, 1);
    set_mangled_mode();

    // スプライトモード
    set_sprite_mode(sprite_large);

    // キークリックスイッチ(OFF)
    *(uint8_t *)MSX_CLIKSW = 0;

    // 画面クリア
    fill(VRAM_START, VRAM_NONE, VRAM_WIDTH * VRAM_HEIGHT);

    // PCG 設定(3面に同じデーターを転送)
    vwrite(pcg_char, 0x0000, 0x800);
    vwrite(pcg_char, 0x0800, 0x800);
    vwrite(pcg_char, 0x1000, 0x800);

    // 色設定(3面に同じデーターを転送)
    vwrite(pcg_color, 0x2000, 0x800);
    vwrite(pcg_color, 0x2800, 0x800);
    vwrite(pcg_color, 0x3000, 0x800);

    // スプライト設定
    set_sprite_16(0, sprite_char1_color1);
    set_sprite_16(1, sprite_char1_color2);
    set_sprite_16(2, sprite_char2_color1);
    set_sprite_16(3, sprite_char2_color2);
}

#ifndef __INTELLISENSE__
void write_psg(uint8_t reg, uint8_t dat)
{
#asm
    LD   HL , 2    ;
    ADD  HL, SP    ; skip over return address on stack
    LD   E, (HL)   ; WRTPSG(E)
    INC  HL        ; skip value call stack
    INC  HL        ;
    LD   A,(HL)    ; WRTPSG(A)
    CALL $0093     ; call WRTPSG(A, E)
#endasm
}
#endif

/**
 * 特殊効果音
 */
void sound_jump()
{
    // 発音なしもしくはファンファーレ発音中
    if(game.sound_jump_index == 0 && game.sound_play == 1) {
        // ボリューム設定
        write_psg(0xa, 0x0);
        return;
    }
    // インデックス
    uint8_t index = game.sound_jump_index - 1;
    // 発音が最後まできた
    if(sound_external_jump[index] == 0xff || index >= 60) {
        game.sound_jump_index = 0;
        return;
    }
    // トーン取得
    uint16_t tone = sound_external_jump[index];
    // ファンファーレ再生中は鳴らさない
    if(game.sound_play != 2) {
        // ボリューム設定
        write_psg(0xa, 14);
        // トーン設定
        write_psg(0x4, tone & 0xff);
        write_psg(0x5, (tone & 0x0f00) >> 8);
    }

    // インデックス増加
    game.sound_jump_index ++;
}

/**
 * スコア表示
 */
void print_score()
{
    uchar score_string[8];
    // ハイスコア
    sprintf(score_string, "%06u0", game.score_hi);
    vwrite(score_string, VPOS(25, 2), 7);
    // スコア
    sprintf(score_string, "%06u0", game.score);
    vwrite(score_string, VPOS(25, 7), 7);
}

/**
 * パワーゲージ表示
 */
void print_power()
{
    int8_t power = game.power;
    // 最大桁数アロケート
    uchar power_string[9];
    memset(power_string, VRAM_NONE, sizeof(power_string));
    // 上限表現
    power_string[0] = 'y';
    // パワー上限は 8 * 7 = 56(GAME_POWER_MAX) (0除算は 0)
    uint8_t block = game.power / 8;
    memset(&power_string[8 - block], 'z', block);
    uint8_t detail = game.power % 8;
    if(detail != 0) {
        power_string[8 - block - 1] = 'z' - detail;
    }
    vwrite(power_string, VPOS(24, 12), 8);
}

/**
 * クリア条件表示
 */
void print_extend()
{
    uchar extend_string[4];
    // レベル
    sprintf(extend_string, "%3u", game.level);
    vwrite(extend_string, VPOS(29, 17), 3);
    // 階数
    sprintf(extend_string, "%3u", GAME_EXTEND_FLOOR - game_stage.climbed_pos);
    vwrite(extend_string, VPOS(29, 22), 3);
}

/**
 * 足場生成
 */
void create_scaffold(uint8_t y)
{
    uint8_t level = game.level_internal;

    uint8_t hole_width = 4;
    uint8_t floor_interval = 4;

    // フロアインターバル判定
    uint8_t interval = 0;
    // 下のフロアーの構造
    uint32_t lower_floor = 0;
    for(uint8_t yy = y + 1; yy <= STAGE_HEIGHT; yy++) {
        lower_floor = game_stage.scaffold[yy];
        if(lower_floor != 0) break;
        interval++;
    }

    // フロアが一定数離れていなければフロアはつくらない
    if(interval <= floor_interval) {
        game_stage.scaffold[y] = 0x0;
        return;
    }

    // エクステンドフロアは埋める
    if(game_stage.climbed_pos++ == 0) {
        game_stage.scaffold[y] = GAME_SCAFFOLD_FILL;
        return;
    }

    // 穴生成
    uint32_t hole = GAME_SCAFFOLD_FILL;
    for(uint8_t xx = 0; xx < STAGE_WIDTH; xx++) {
        // 下のフロアの足場状態取得
        uint8_t lower_hole = (lower_floor >> xx) & /* 良い足場 */ 0xf;
        // 下のフロアに良い足場があれば x/22 の確率で穴を生成
        if(lower_hole != 0 && get_rnd() % STAGE_WIDTH <= level) {
            hole ^= (uint32_t)((2 << (hole_width - 1)) - 1) << xx;
            xx += hole_width + /* TODO: */ 3;
        }
    }
    game_stage.scaffold[y] = hole;
}

/**
 * 足場表示（1行）
 */
void print_scaffold_line(uint8_t y, uint32_t scaffold, uint8_t char_code)
{
    uint8_t x_vram[STAGE_WIDTH - 2];
    // メモリーを空白で初期化
    memset(x_vram, 'b', STAGE_WIDTH - 2);

    // 32bit シフトで判定すると遅いので 8bit に分割
    uint8_t *scaffold_byte = (uint8_t *)&scaffold;

    // 足場を走査して描画(1byte目)
    if(scaffold_byte[0] == 0xff) {
        memset(&x_vram[0], char_code, 8);
    } else {
        for(uint8_t x = 1; x <= 8; x++) {
            if(scaffold_byte[0] & 0x1 == 1) {
                x_vram[x - 1] = char_code;
            }
            scaffold_byte[0] >>= 1;
        }
    }
    // 足場を走査して描画(2byte目)
    if(scaffold_byte[1] == 0xff) {
        memset(&x_vram[8], char_code, 8);
    } else {
        for(uint8_t x = 9; x <= 16; x++) {
            if(scaffold_byte[1] & 0x1 == 1) {
                x_vram[x - 1] = char_code;
            }
            scaffold_byte[1] >>= 1;
        }
    }
    // 足場を走査して描画(3byte目)
    if(scaffold_byte[2] == 0x3f) {
        memset(&x_vram[16], char_code, 6);
    } else {
        for(uint8_t x = 17; x <= 22; x++) {
            if(scaffold_byte[2] & 0x1 == 1) {
                x_vram[x - 1] = char_code;
            }
            scaffold_byte[2] >>= 1;
        }
    }

    // ブロック転送
    vwrite(x_vram, VPOS(1, y), STAGE_WIDTH - 2);
}

/**
 * 足場表示（範囲指定）
 */
void print_scaffold(uint8_t range_y)
{
    for(uint8_t y = 0; y <= range_y; y++) {
        // スクロール範囲外は表示しない
        if(y == 0) continue;
        // 仮想足場上、足場がなければ処理しない
        if((game_stage.scaffold[y] & GAME_SCAFFOLD_FILL) == 0) continue;
        // 足場を VRAM に書き込み
        print_scaffold_line(y - 1, game_stage.scaffold[y], 'a');
    }
}

/**
 * 足場ドットスクロール
 */
void scroll_scaffold_line(void)
{
    // アジャストがなければ何もしない
    if(game_stage.scroll_adjust == 0) return;

    uint8_t adjust = game_stage.scroll_adjust * 2;
    // スクロール後の上キャラ
    uint8_t upper = 'a' + adjust;
    // スクロール後の下キャラ
    uint8_t lower = 'b' + adjust;
    // アジャストが 8 ならそのまま下に転送
    if(game_stage.scroll_adjust == 8) {
        upper = 'b';
        lower = 'a';
    }
    // スクロール範囲外描画
    uint32_t out_of_scaffold = game_stage.scaffold[0] & GAME_SCAFFOLD_FILL;
    if(out_of_scaffold != 0) {
        print_scaffold_line(0, out_of_scaffold, lower);
    }
    // VRAM 走査
    uint8_t x_vram[STAGE_WIDTH - 2];
    uint8_t x_vram_trance_upper[STAGE_WIDTH - 2];
    uint8_t x_vram_trance_lower[STAGE_WIDTH - 2];
    for(uint8_t y = 0; y < STAGE_HEIGHT; y++) {
        // 仮想 VRAM 上、足場がなければ何もしない
        if((game_stage.scaffold[y + 1] & GAME_SCAFFOLD_FILL) == 0) continue;
        // スクロール範囲外描画が先頭にかかっている時は何もしない
        // フロア間に interval が必ず 1 行以上存在する
        if(y == 0 && out_of_scaffold != 0) continue;
        // 足場がある部分とその下のキャラをスクロールキャラに置き換え
        uint16_t addr = VPOS(1, y);
        // メインメモリーに VRAM をブロック転送
        vread(addr, x_vram, STAGE_WIDTH - 2);
        // 転送先初期化
        memset(x_vram_trance_upper, 'b', STAGE_WIDTH - 2);
        memset(x_vram_trance_lower, 'b', STAGE_WIDTH - 2);
        // メインメモリー上でアジャストに合わせて表示キャラを構築
        for(uint8_t x = 0; x < STAGE_WIDTH - 2; x++) {
            if(x_vram[x] != 'b') {
                x_vram_trance_upper[x] = upper;
                x_vram_trance_lower[x] = lower;
            }
        }
        // メインメモリーから VRAM に転送
        vwrite(x_vram_trance_upper, addr, STAGE_WIDTH - 2);
        if(y < STAGE_HEIGHT - 1) {
            vwrite(x_vram_trance_lower, addr + VRAM_WIDTH, STAGE_WIDTH - 2);
        }
    }
}

/**
 * 足場スクロール
 */
void scroll_scaffold()
{
    // 1ラインスクロール
    scroll_scaffold_line();
    // アジャスト加算
    game_stage.scroll_adjust += 2;
    // アジャスト完了
    if(game_stage.scroll_adjust > 8) {
        game_stage.scroll_adjust = 0;
        // この時点で足場の表示は下の VRAM に転送済み
        // 仮想 VRAM をスクロール
        memmove(&game_stage.scaffold[1], &game_stage.scaffold[0], sizeof(uint32_t) * (STAGE_HEIGHT));
        // スクロール外の足場生成
        create_scaffold(0);
    }
}

/**
 * パワーゲージ算出
 */
void calc_power() {
    // ゲージ増分は最小最大で反転する
    if(game.power >= GAME_POWER_MAX || game.power <= GAME_POWER_MIN) {
        game.power_v *= -1;
    }
    game.power += game.power_v;

    // power_v を考慮してパワーゲージに 0(GAME_POWER_MIN) - 56(GAME_POWER_MAX) でキャップをかける
    if(game.power >= GAME_POWER_MAX) {
        game.power = GAME_POWER_MAX;
    } else if(game.power <= GAME_POWER_MIN) {
        game.power = GAME_POWER_MIN;
    }
}

/**
 * 自機スプライト表示
 */
void sprite_my()
{
    uint8_t sprite_no = 2;
    int16_t sprite_x = my.x;
    int16_t sprite_y = my.y;

    // スプライト番号決定
    if(my.vx < 0) {
        sprite_no = 0;
    }
    // 画面範囲外なら部分表示フラグを立てる
    if(my.y < 0) {
        if(my.y >= -16) {
            // はみ出し表示可能
            sprite_y = 0xf0 | 16 + my.y;
        } else {
            // はみ出せないので表示しない
            put_sprite_16(my.sprite_no, 0, HIDDEN_SPRITE_Y, my.sprite_no, 1);
            put_sprite_16(my.sprite_no + 1, 0, HIDDEN_SPRITE_Y, my.sprite_no + 1, 0xc);
            return;
        }
    }

    // スプライトが異なれば消す
    if(my.sprite_no != sprite_no) {
        put_sprite_16(my.sprite_no, 0, HIDDEN_SPRITE_Y, my.sprite_no, 1);
        put_sprite_16(my.sprite_no + 1, 0, HIDDEN_SPRITE_Y, my.sprite_no + 1, 0xc);
    }
    my.sprite_no = sprite_no;

    // 2色スプライト
    put_sprite_16(my.sprite_no, sprite_x, sprite_y, my.sprite_no, 1);
    put_sprite_16(my.sprite_no + 1, sprite_x, sprite_y, my.sprite_no + 1, 0xc);
}

/**
 * 自機の移動処理
 */
void move_my(uint8_t jump_power)
{
    // 自機は常に左右に移動
    my.x += my.vx;
    if(my.x < 8 || my.x > 176 - 8) {
        my.vx *= -1;
    }

    // ジャンプパワー補正
    uint8_t adjust_jump_power = 0;
    if(jump_power > 2) {
        adjust_jump_power = 5 + jump_power / 4;
    }

    // ジャンプ可能でジャンプパワーがある場合はジャンプ開始
    if(my.can_jump && adjust_jump_power != 0) {
        // ジャンプ中はジャンプできない
        my.can_jump = false;
        // 放物線初期化
        my.jump_increment = -(int16_t)adjust_jump_power;
    }

    // ジャンプ落下もしくは歩き落下中
    if(my.can_jump == false) {
        // 放物線算出
        int16_t y_temp = my.y;
        int16_t diff = (int16_t)((my.y - my.y_prev) + my.jump_increment);
        // 壁判定抜け抑止のため 8 ドット以上は差分に取らない
        if(diff >= 8) diff = 8;
        my.y += diff;
        my.y_prev = y_temp;
        my.jump_increment = 1;
    }

    // VRAM 上の位置算出
    uint8_t vram_x = (my.x + /* harf sprite size */ 8) / 8;
    uint8_t vram_y = (my.y + /* harf sprite size */ 16) / 8;
    // 画面上に -16 はみ出しであれば 0 に補正
    if(my.y <= 0 && my.y >= -16) vram_y = 0;
    // 判定キャラ
    uint16_t addr = VPOS(vram_x, vram_y);
    uint8_t judge_char = vpeek(addr);
    // 画面上 -8 よりはみ出ていたら足場は存在しない
    if(my.y < -8 || my.y > 184) judge_char = 'b';
    // スクロールアジャスト
    uint8_t scroll_adjust = game_stage.scroll_adjust;

    // 歩き中に下に床がなければ歩き落下
    if(my.can_jump && adjust_jump_power == 0 && judge_char == 'b') {
        // 歩き落下中はジャンプできない
        my.can_jump = false;
    }

    // ジャンプ・歩き落下中判定
    int8_t diff_y = (int8_t)(my.y_prev - my.y);
    // 画面内で落下中の場合は足場を判定
    if(diff_y < 0 && my.y >= -16) {
        // 落下中は足場判定を左右に大きくする救済判定
        bool rescue_x = false;
        uint8_t rescue_char;
        uint8_t rescue_r = vpeek(addr + 1);
        uint8_t rescue_l = vpeek(addr - 1);
        if(my.vx > 0 && rescue_r != 'b') {
            rescue_x = true;
            rescue_char = rescue_r;
        } else if(my.vx < 0 && rescue_l != 'b') {
            rescue_x = true;
            rescue_char = rescue_l;
        }
        // 下足場判定
        if(judge_char != 'b' || rescue_x) {
            // ジャンプ及び y 座標を初期化
            my.can_jump = true;
            // 着位置 y 座標補正
            my.y = vram_y * 8 - 16 + scroll_adjust;
            // 埋まりアジャスト
            int8_t adjust = judge_char - 'a';
            if(rescue_x) adjust = rescue_char - 'a';
            if(adjust % 2 == 1 && adjust >=1 && adjust <= 15) {
                my.y -= 8 - adjust / 8;
            }
            my.y_prev = my.y;
            // 自由落下ベクトルを初期化
            my.vy = 0;
            // ジャンプ効果音停止
            game.sound_jump_index = 0;
        }
    } else if(my.can_jump && adjust_jump_power == 0 && game_stage.scroll_adjust > 0) {
        // 歩き中は強制スクロール分調整
        my.y = vram_y * 8 - 16 + scroll_adjust;
    }

    // スプライト表示
    sprite_my();
}

/**
 * ゲーム状態初期化
 */
void init_game_state()
{
    // 自機の状態
    my.x = MY_INIT_X;
    my.y = MY_INIT_Y;
    my.vx = MY_INIT_VX;
    my.vy = MY_INIT_VY;
    my.x_prev = my.x;
    my.y_prev = my.y;
    my.can_jump = true;
    my.jump_increment = 0;

    // ゲーム状態
    game.level = GAME_INIT_LEVEL;
    game.level_internal = GAME_INIT_LEVEL;
    game.power = GAME_INIT_POWER;
    game.power_v = GAME_INIT_POWER_V;
    game.power_trigger = GAME_INIT_POWER_TRIGGER;
    game.jump_extention_tick = 0;
    game.sound_play = 0;
    game.score = 0;
    game.sound_jump_index = 0;

    // ステージ状態
    game_stage.scroll_adjust = 0;
    game_stage.climbed_pos = 0;

    // 最下層の足場生成
    game_stage.scaffold[STAGE_HEIGHT] = GAME_SCAFFOLD_FILL;
    // 下の足場との距離を測るため下から上に足場生成
    for(int8_t y = STAGE_HEIGHT - 1; y >= 0; y--) {
        create_scaffold(y);
    }
}

/**
 * ゲーム画面初期化
 */
void screen_init()
{
    // 画面クリア
    fill(VRAM_START, VRAM_NONE, VRAM_WIDTH * VRAM_HEIGHT);

    // 横壁
    for(uint8_t y = 0; y < STAGE_HEIGHT; y++) {
        vpoke(VPOS(0, y), '`');
        vpoke(VPOS((STAGE_WIDTH - 1), y), 'q');
        fill(VPOS(1, y), 'b', (STAGE_WIDTH - 2));
    }

    // 右ペイン
    vwrite("HISCORE", VPOS(25,  0), 7);
    vwrite("  SCORE", VPOS(25,  5), 7);
    vwrite("  POWER", VPOS(25, 10), 7);
    vwrite("  LEVEL", VPOS(25, 15), 7);
    vwrite(" REMAIN", VPOS(25, 20), 7);

    // ステート表示
    print_score();
    print_power();
}

/**
 * ステージ上の文字描画
 */
void print_string_screen(char *string, uint8_t len, uint8_t x, uint8_t y)
{
    uint8_t i = 0;
    uint8_t vram[VRAM_WIDTH];
    while(i < len) {
        uint8_t char_code = string[i];
        if(char_code == 0x20) {
            char_code = 'b';
        } else if(char_code > 0x20 && char_code <= '_') {
            char_code += 0x60;
        }
        vram[i] = char_code;
        i++;
    }
    vwrite(vram, VPOS(x, y), len);
}

/**
 * タイトル表示及びアドバタイズデモ用初期化
 */
void title_init()
{
    // サウンド再生開始
    sounddrv_bgmplay(music_title);

    // 画面クリア
    fill(VRAM_START, 'b', VRAM_WIDTH * VRAM_HEIGHT);

    // アドバタイズデモ用初期化
    init_game_state();

    // アドバダイズデモ画面表示
    screen_init();

    // 初期足場表示
    print_scaffold(STAGE_HEIGHT);

    // 足場残り初期化
    game_stage.climbed_pos = 0;
    print_extend();

    // タイトル表示
    print_string_screen("                ", 16, 4,  9);
    print_string_screen("   NOBORUNOCA   ", 16, 4, 10);
    print_string_screen("                ", 16, 4, 11);
    print_string_screen("                ", 16, 4, 12);
    print_string_screen(" HIT SPACE KEY! ", 16, 4, 13);
    print_string_screen("                ", 16, 4, 14);

    // アドバタイズデモに遷移
    game.state = TITLE_ADVERTISE;
}

/**
 * タイトルアドバタイズデモ
 */
void title_advertise(uint8_t trigger)
{
    // フレーム間先行入力
    // ゲームオーバー後の連続入力無視のため最初の 30 tick は入力を受け付けない
    if(title.tick > 30 && trigger) {
        title.trigger_state = trigger;
    } else {
        title.trigger_state = 0;
    }

    // HIT SPACE KEY
    if(title.trigger_state) {
        // 乱数シード初期化（パワーゲージ目押しで選択できる）
        seed_rnd(game.power);
        // ゲーム初期化に遷移
        game.state = GAME_INIT;
        return;
    }

    // 3フレーム更新
    if(title.tick++ % 3 != 0) return;

    // 良さそうな時に気ままにジャンプ
    uint8_t jump_power = 0;
    if(my.can_jump && my.y > 80 && get_rnd() % 30 == 0 && game.power > 20) {
        jump_power = game.power;
    }

    // 自機移動
    move_my(jump_power);

    // アドバダイズ中は常にジャンプトリガー
    calc_power();

    // パワーゲージデモ表示
    print_power();

    title.tick++;
}

/**
 * ゲーム初期化
 */
void game_init()
{
    // サウンド停止
    sounddrv_stop();

    // 画面クリア
    fill(VRAM_START, 0x20, VRAM_WIDTH * VRAM_HEIGHT);

    // ゲーム用初期化
    init_game_state();

    // ゲーム画面表示
    screen_init();

    // 初期足場表示
    print_scaffold(STAGE_HEIGHT);

    // 足場残り初期化
    game_stage.climbed_pos = 0;
    print_extend();

    // ジャンピングスタート
    game.jump_extention_tick = GAME_JUMP_EXTENTION_FRAME;
    game.power = 40;

    // ゲームに遷移
    game.state = GAME_MAIN;
}

/**
 * ゲームオーバー
 */
void game_over(uint8_t trigger)
{
    // HIT SPACE KEY（60フレームは受け付けない）
    if(trigger && game.tick > 60) {
        // タイトルに遷移
        game.state = TITLE_INIT;
    }

    print_string_screen("                ", 16, 4,  9);
    print_string_screen("   GAME OVER    ", 16, 4, 10);
    print_string_screen("                ", 16, 4, 11);
    print_string_screen("                ", 16, 4, 12);
    print_string_screen(" HIT SPACE KEY! ", 16, 4, 13);
    print_string_screen("                ", 16, 4, 14);

    game.tick++;
}

/**
 * ゲームメイン
 */
void game_main(uint8_t trigger)
{
    // フレーム間先行入力
    if(trigger) {
        game.trigger_state = trigger;
    } else {
        game.trigger_state = 0;
    }

    // ファンファーレ系再生終わり検知
    if(game.sound_play == 2 && (sounddrv_bgmwk[3] + sounddrv_bgmwk[4]) == 0) {
        game.sound_play = 0;
    }
    // メインミュージック再生
    if(game.sound_play == 0) {
        sounddrv_bgmplay(music_main);
        // メインミュージック再生中
        game.sound_play = 1;
    }

    // ジャンプ効果音
    sound_jump();

    // ゲームスピード調整（レベル 10 以降はスピードアップ）
    if(game.level < 10 && game.tick++ % 3 == 0) return;

    // 足場スクロール
    scroll_scaffold(game.level);

    // ジャンプ猶予フレーム算出
    if(game.jump_extention_tick > 0) {
        game.jump_extention_tick--;
    }

    // パワーゲージ表示
    print_power();

    // ジャンプパワー決定
    uint8_t jump_power = 0;

    // トリガーされたらパワーゲージスタート
    if(game.trigger_state) {
        game.power_trigger = game.trigger_state;
        calc_power();
    }
    // トリガー中に離されたらジャンプ処理開始
    if(game.power_trigger && !game.trigger_state) {
        if(my.can_jump) {
            // ジャンプパワー決定
            jump_power = game.power;
            game.jump_extention_tick = 0;
            // ジャンプボーナス
            game.score += 2;
            // ジャンプ効果音開始
            game.sound_jump_index = 1;
        } else {
            // 連続ジャンプトリガー猶予設定
            game.jump_extention_tick = GAME_JUMP_EXTENTION_FRAME;
            game.power_trigger = false;
        }
    }

    // 連続ジャンプトリガー猶予ジャンプ
    if(game.jump_extention_tick > 0) {
        // 猶予フレーム内に落下しなければジャンプは成立しない
        if(game.jump_extention_tick <= 1) {
            // ジャンプパワー初期化
            game.power = GAME_INIT_POWER;
            jump_power = game.power;
            game.jump_extention_tick = 0;
        }
    }
    // 猶予フレーム内ならジャンプ開始
    if(my.can_jump && game.jump_extention_tick > 0) {
        // ジャンプ効果音開始
        game.sound_jump_index = 1;
        // ジャンプパワー初期化
        jump_power = game.power;
        game.power = GAME_INIT_POWER;
        game.jump_extention_tick = 0;
        // 目押しジャンプボーナス
        game.score += 5;
    }

    // 自機移動とジャンプ
    move_my(jump_power);

    // 自機やられ判定
    if(my.y >= 184) {
        // サウンド停止
        sounddrv_stop();
        // やられサウンド再生
        game.sound_play = 2;
        sounddrv_bgmplay(music_game_over_2);
        // ゲームオーバーに遷移
        game.tick = 0;
        game.state = GAME_OVER;
    }

    // 登りボーナス
    if(game.tick % 60 == 0) {
        game.score += 5;
    }
    // ハイスコア更新
    if(game.score_hi < game.score) {
        game.score_hi = game.score;
    }

    // レベルエクステンド
    if(game_stage.climbed_pos > GAME_EXTEND_FLOOR) {
        // レベルエクステンドサウンド再生
        game.sound_play = 2;
        sounddrv_bgmplay(sound_extend);
        // 次回エクステンド更新（全埋足場生成）
        game_stage.climbed_pos = 0;
        game.level++;
        game.level_internal = game.level;
        // 足場構造を決定する内部レベルは 4 で上限
        if(game.level > 4) {
            game.level_internal = 4;
        }
        // レベルボーナス
        game.score += 30;
        // レベル更新
        print_extend();
    }

    // スコア表示(遅くなるので 30フレームごと)
    if(game.tick % 30 == 0) {
        print_score();
        print_extend();
    }
}

/**
 * ゲームループ及び状態遷移
 */
void loop()
{
    while(1) {
        // vsync wait
        wait_vsync(1);
        // 入力取得
        uint8_t trigger = get_trigger(0);
        // ゲーム
        switch(game.state) {
            case TITLE_INIT:
                title_init();
                break;
            case TITLE_ADVERTISE:
                title_advertise(trigger);
                break;
            case GAME_INIT:
                game_init();
                break;
            case GAME_MAIN:
                game_main(trigger);
                break;
            case GAME_OVER:
                game_over(trigger);
                break;
            default:
                break;
        }
    }
}

/**
 * メイン
 */
void main()
{
    // 画面初期化
    init_graphics();

    // サウンドドライバー初期化
    sounddrv_init();
    // サウンドドライバーフック設定
    // __INTELLISENSE__ 判定は vscode で非標準インラインアセンブル構文をエラーにしないように挿入
    #ifndef __INTELLISENSE__
    __asm
    DI
    __endasm;
    #endif
    uint8_t *h_time = (uint8_t *)MSX_H_TIMI;
    uint16_t hook = (uint16_t)sounddrv_exec;
    h_time[0] = 0xc3; // JP
    h_time[1] = (uint8_t)(hook & 0xff);
    h_time[2] = (uint8_t)((hook & 0xff00) >> 8);
    #ifndef __INTELLISENSE__
    __asm
    EI
    __endasm;
    #endif

    // ゲームステート初期化
    game.state = TITLE_INIT;
    game.score_hi = GAME_INIT_HISCORE;

    // start
    loop();
}
