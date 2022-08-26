#include "gpu.h"
#include "cpu.h"

typedef struct {
	/* LCD CONTROL REGISTER */
	uint8_t lcd_control;
	uint8_t lcd_stat;

	/* SCROLL REGISTERS */
	uint8_t scrolly;
	uint8_t scrollx;
	uint8_t curline;
	uint8_t cmpline;
	uint8_t prevline;

	/* PALETTES */
	uint8_t bgrdpalette[4];
	uint8_t palette0[4];
	uint8_t palette1[4];

	/* WINDOW POSITIONS */
	uint8_t wndposy;
	uint8_t wndposx;
	uint8_t wndlinecnt;

	int scanline_counter;

	uint8_t obj_buffer[10];
	int obj_buffer_size;
} gpu_state;

enum {
	CTRL_BG_WIN_ENABLE      = 0x1,
	CTRL_SPRITES_ENABLE     = 0x2,
	CTRL_SPRITES_SIZE       = 0x4,
	CTRL_BG_WIN_MAP_SELECT  = 0x8,
	CTRL_BG_WIN_TILE_SELECT = 0x10,
	CTRL_WIN_ENABLE         = 0x20,
	CTRL_WIN_MAP_SELECT     = 0x40,
	CTRL_RENDER_ENABLE      = 0x80
};

enum {
	STAT_MODE_BLANK_FLAG      = 0x1,
	STAT_MODE_MEM_ACCESS_FLAG = 0x2,
	STAT_MODE_MASK            = 0x3,
	STAT_COINCIDENCE_FLAG     = 0x4,
	STAT_H_BLANK_INT_FLAG     = 0x8,
	STAT_V_BLANK_INT_FLAG     = 0x10,
	STAT_OAM_INT_FLAG         = 0x20,
	STAT_COINCIDENCE_INT_FLAG = 0x40
};

enum {
	OAM_FIRST_PALETTE = 0x10,
	OAM_X_FLIP_FLAG   = 0x20,
	OAM_Y_FLIP_FLAG   = 0x40,
	OAM_PRIORITY_FLAG = 0x80
};

enum {
	MODE_HBLANK = 0,
	MODE_VBLANK = 1,
	MODE_ACCESS_OAM = 2,
	MODE_ACCESS_VRAM = 3,
};

static int mode; 
static gpu_state state;
static uint8_t   vram[0x2000]; // video ram, 8 kbytes
static uint8_t   oam[0xA0]; // oam ram
// TOOD: debug issue with buffer overflow, this ugly hack fixes it
static uint8_t   canvas[(SCREEN_WIDTH + 1) * (SCREEN_HEIGHT + 1)];

static void gpu_canvas_put_pixel (int x, int y, uint8_t color);

static uint8_t gpu_canvas_get_pixel (int x, int y);

static void gpu_canvas_render (void);

static void gpu_render_bg (int scanline);

static void gpu_render_window (int scanline);

static void gpu_scan_sprite_lines (int scanline);

static void gpu_render_sprites_from_buffer (void);

static void gpu_render_sprite (int sprite);

static uint8_t inline translate_color (int color, uint8_t *palette);

static uint8_t inline color_to_default_palette (int color);

static void inline parse_colors_from_bit_palette (uint8_t palette, uint8_t *palette_save);

#ifdef DEBUG_BUILD

#define DEBUG_WINDOW_WIDTH (16 * 8)
#define DEBUG_WINDOW_HEIGHT (24 * 8)
#define DEBUG_WINDOW_RATIO (3)

static const bool debug = false;
static SDL_Window *wind = NULL;
static SDL_Renderer *rend = NULL;

static void gpu_debug(void);

static void gpu_debug_put_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
	if (x < 0 || x > DEBUG_WINDOW_WIDTH || y < 0 || y > DEBUG_WINDOW_HEIGHT) {
		return;
	}

	SDL_SetRenderDrawColor(rend, r, g, b, 255);
	SDL_RenderDrawPoint(rend, x, y);
}

static void gpu_debug_render(void) {
	SDL_RenderPresent(rend);
}

static void gpu_debug_clear(void) {
	SDL_SetRenderDrawColor(rend, 255, 255, 255, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(rend);
}
#endif /* debug end */

void gpu_init (void) {
#ifdef DEBUG_BUILD
	SDL_CreateWindowAndRenderer(DEBUG_WINDOW_WIDTH * DEBUG_WINDOW_RATIO, DEBUG_WINDOW_HEIGHT * DEBUG_WINDOW_RATIO, SDL_WINDOW_UTILITY, &wind, &rend);
	SDL_SetWindowTitle(wind, "DEBUG TILE WINDOW");
	SDL_RenderSetLogicalSize(rend, DEBUG_WINDOW_WIDTH, DEBUG_WINDOW_HEIGHT);
	SDL_RenderSetScale(rend, (float)DEBUG_WINDOW_RATIO, (float)DEBUG_WINDOW_RATIO);
	gpu_debug_clear();
#endif /* debug end */

	memset(oam, 0x00, 0xA0);
	memset(&canvas, 0x00, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint8_t));
	memset(&state.obj_buffer, 0x00, sizeof(uint8_t) * 10);
	state.scanline_counter = 456;
	state.curline = 0;
	state.obj_buffer_size = 0;

	screen_clear();
	screen_vsync();
}

void gpu_step(int cycles) {
	uint8_t status            = state.lcd_stat;
	int     current_mode      = state.lcd_stat & STAT_MODE_MASK;

	if (!(state.lcd_control & CTRL_RENDER_ENABLE)) {
		screen_clear();
		screen_vsync();
		state.scanline_counter = 456;
		state.curline          = 0;
		status &= 252;
		status &= ~STAT_MODE_BLANK_FLAG;
		status &= ~STAT_MODE_MEM_ACCESS_FLAG;
		state.lcd_stat = status;
		return;
	}

	if (state.curline >= 144 && state.curline <= 153) {
		status |= STAT_MODE_BLANK_FLAG;
		status &= ~STAT_MODE_MEM_ACCESS_FLAG;
	} else {
		if (state.scanline_counter <= 80) {
			status &= ~STAT_MODE_BLANK_FLAG;
			status |= STAT_MODE_MEM_ACCESS_FLAG;
		} else if (state.scanline_counter > 80 && state.scanline_counter <= 289) {
			status |= STAT_MODE_BLANK_FLAG;
			status |= STAT_MODE_MEM_ACCESS_FLAG;
		} else {
			status &= ~STAT_MODE_BLANK_FLAG;
			status &= ~STAT_MODE_MEM_ACCESS_FLAG;
		}
	}

	if (state.prevline != state.curline) {
		if (state.curline == state.cmpline) {
			status |= STAT_COINCIDENCE_FLAG;

			if (status & STAT_COINCIDENCE_INT_FLAG) {
				cpu_request_interrupt(1);
			}
		} else {
			status &= ~STAT_COINCIDENCE_FLAG;
		}
	}

	state.lcd_stat = status;

	if (current_mode != (status & STAT_MODE_MASK)) {
		switch (status & STAT_MODE_MASK) {
			case MODE_ACCESS_OAM:
				gpu_scan_sprite_lines(state.curline);

				if (status & STAT_OAM_INT_FLAG) {
					cpu_request_interrupt(1);
				}

				break;
			case MODE_ACCESS_VRAM:
				if (state.lcd_control & CTRL_BG_WIN_ENABLE) {
					gpu_render_bg(state.curline);

					if (state.lcd_control & CTRL_WIN_ENABLE) {
						gpu_render_window(state.curline);
					}
				}
				gpu_render_sprites_from_buffer();
				break;
			case MODE_HBLANK:
				if (status & STAT_H_BLANK_INT_FLAG) {
					cpu_request_interrupt(1);
				}
				break;
			case MODE_VBLANK:
				gpu_canvas_render();
				screen_vsync();

				if (status & STAT_V_BLANK_INT_FLAG) {
					cpu_request_interrupt(1);
				} else {
					cpu_request_interrupt(0);
				}

#ifdef DEBUG_BUILD
				gpu_debug();
#endif
				break;
			default:
				printl("WTF?????????");
				break;
		}
	}

	state.scanline_counter += cycles;
	state.prevline = state.curline;

	if (state.scanline_counter >= 456) {
		state.curline++;
		state.scanline_counter = 0;
	}

	if (state.curline > 153) {
		state.curline = 0;
		state.wndlinecnt = 0;
	}
}

static void gpu_canvas_put_pixel (int x, int y, uint8_t color) {
	if (x < 0 || x > SCREEN_WIDTH || y < 0 || y > SCREEN_HEIGHT) {
		return;
	}

	canvas[y * SCREEN_WIDTH + x] = color;
}

static uint8_t gpu_canvas_get_pixel (int x, int y) {
	return canvas[y * SCREEN_WIDTH + x];
}

static void gpu_canvas_render (void) {
	for (int x = 0; x < SCREEN_WIDTH; x++) {
		for (int y = 0; y < SCREEN_HEIGHT; y++) {
			uint8_t color = canvas[y * SCREEN_WIDTH + x];
			screen_put_pixel(x, y, color, color, color);
		}
	}
}

static void gpu_scan_sprite_lines (int scanline) {
	const uint8_t sprite_size = (state.lcd_control & CTRL_SPRITES_SIZE) ? 2 : 1;

	state.obj_buffer_size = 0;

	if (!(state.lcd_control & CTRL_SPRITES_ENABLE)) {
		return;
	}

	for (int sprite = 0; sprite < 40; ++sprite) {
		uint16_t sprite_offset = sprite*4;

		uint8_t sprite_y = oam[sprite_offset + 0];
		uint8_t sprite_x = oam[sprite_offset + 1];

		if (sprite_x == 0) {
			continue;
		}

		if (scanline + 16 >= sprite_y && scanline + 16 < (sprite_y + sprite_size * 8) && state.obj_buffer_size < 10) {
			// FIXME: use sort here to fix X priority problem
			state.obj_buffer[state.obj_buffer_size++] = sprite;
		}
	}
}

static void gpu_render_sprites_from_buffer (void) {
	if (!(state.lcd_control & CTRL_SPRITES_ENABLE)) {
		return;
	}

	for (int i = 0; i < state.obj_buffer_size; ++i) {
		gpu_render_sprite(state.obj_buffer[i]);
	}

	state.obj_buffer_size = 0;
}

static void gpu_render_sprite (int sprite) {
	const uint8_t sprite_size = (state.lcd_control & CTRL_SPRITES_SIZE) ? 2 : 1;
	const uint16_t sprite_offset = sprite*4;

	uint8_t sprite_y = oam[sprite_offset + 0];          // y pos
	uint8_t sprite_x = oam[sprite_offset + 1];          // x pos
	uint8_t sprite_n = oam[sprite_offset + 2];          // number in tile table
	uint8_t sprite_a = oam[sprite_offset + 3];          // attribute bit array

	// position for right bottom pixel, so we must subtract 8/16 px for correct rendering
	bool use_first_palette = (sprite_a & OAM_FIRST_PALETTE) ? true : false;
	bool flip_x            = (sprite_a & OAM_X_FLIP_FLAG) ? true : false;
	bool flip_y            = (sprite_a & OAM_Y_FLIP_FLAG) ? true : false;
	bool lower_prio        = (sprite_a & OAM_PRIORITY_FLAG) ? true : false;

	if (sprite_y == 0 || sprite_y >= SCREEN_HEIGHT + 16) {
		return;
	}

	if (sprite_x == 0 || sprite_x >= SCREEN_WIDTH + 8) {
		return;
	}

	if (state.lcd_control & CTRL_SPRITES_SIZE) {
		sprite_n &= ~1;
	}

	int      screen_y    = sprite_y - 16;
	int      screen_x    = sprite_x - 8;
	uint16_t tile_offset = 0x0000 + sprite_n*16;

	for (uint8_t y = 0; y < (8*sprite_size); ++y) {
		for (uint8_t x = 0; x < 8; ++x) {
			uint8_t ypos = flip_y ? (8*sprite_size) - y - 1 : y;
			uint8_t xpos = flip_x ? 8 - x - 1 : x;

			if (screen_y + y > SCREEN_HEIGHT || screen_x + x > SCREEN_WIDTH) {
				continue;
			}

			uint8_t tile_lo_bit = (vram[tile_offset + ypos*2]>>(8 - xpos - 1)) & 0x1;
			uint8_t tile_hi_bit = (vram[tile_offset + ypos*2 + 1]>>(8 - xpos - 1)) & 0x1;

			int pixel_color = (tile_hi_bit<<1) | tile_lo_bit;

			if (pixel_color == 0) {
				continue;
			}

			uint8_t color = translate_color(pixel_color,
				use_first_palette ? state.palette1 : state.palette0);

			if (lower_prio && gpu_canvas_get_pixel(screen_x + x, screen_y + y) < 255) {
				continue;
			}

			gpu_canvas_put_pixel(screen_x + x, screen_y + y, color);
		}
	}
}

static void gpu_render_window (int scanline) {
	const uint16_t window_base = (state.lcd_control & CTRL_WIN_MAP_SELECT) ? 0x1C00 : 0x1800;
	const uint16_t tile_base   = (state.lcd_control & CTRL_BG_WIN_TILE_SELECT) ? 0x0000 : 0x0800;
	const uint8_t  tile_size   = 16;
	uint8_t scrolled_y = scanline - state.wndposy;

	if (state.wndposy > scanline) {
		return;
	}

	if (state.wndposx >= (SCREEN_WIDTH + 7)) {
		return;
	}

	for (int16_t screen_x = state.wndposx - 7; screen_x < SCREEN_WIDTH; ++screen_x) {
		if (screen_x < 0) { // skip everything on the left under 7 pixels
			continue;
		}

		uint8_t scrolled_x = screen_x - state.wndposx + 7;

		uint8_t tile_x = scrolled_x/8;
		uint8_t tile_y = state.wndlinecnt/8;

		uint8_t  tile_pixel_x = scrolled_x%8;
		uint8_t  tile_pixel_y = scrolled_y%8;
		uint16_t tile_start   = 0;

		if (tile_base == 0x0800) {
			int16_t tile_index = (int8_t) vram[window_base + tile_y*32 + tile_x];
			tile_index += 128;
			tile_start         = tile_base + tile_index*tile_size;
		} else {
			uint8_t tile_index = (uint8_t) vram[window_base + tile_y*32 + tile_x];
			tile_start = tile_base + tile_index*tile_size;
		}

		uint8_t line = tile_pixel_y*2;

		uint8_t tile_lo_bit = (vram[tile_start + line]>>(7 - tile_pixel_x)) & 0x1;
		uint8_t tile_hi_bit = (vram[tile_start + line + 1]>>(7 - tile_pixel_x)) & 0x1;
		int     pixel_color = (tile_hi_bit<<1) | tile_lo_bit;
		uint8_t color       = translate_color(pixel_color, state.bgrdpalette);

		gpu_canvas_put_pixel(screen_x, scanline, color);
	}

	state.wndlinecnt++;
}

static void gpu_render_bg (int scanline) {
	const uint16_t bg_base   = (state.lcd_control & CTRL_BG_WIN_MAP_SELECT) ? 0x1C00 : 0x1800;
	const uint16_t tile_base = (state.lcd_control & CTRL_BG_WIN_TILE_SELECT) ? 0x0000 : 0x0800;
	const uint8_t  tile_size = 16;

	uint8_t ypos     = state.scrolly + scanline;
	int tile_row = ypos/8;

	for (int pixel = 0; pixel < SCREEN_WIDTH; ++pixel) {
		uint8_t  xpos       = pixel + state.scrollx;
		uint8_t  tile_col   = xpos/8;
		uint16_t tile_start = 0;

		if (tile_base == 0x0800) {
			int16_t tile_index = (int8_t) vram[bg_base + tile_row*32 + tile_col];
			tile_index += 128;
			tile_start         = tile_base + tile_index*tile_size;
		} else {
			uint8_t tile_index = (uint8_t) vram[bg_base + tile_row*32 + tile_col];
			tile_start = tile_base + tile_index*tile_size;
		}

		uint8_t line = (ypos%8)*2;

		uint8_t tile_lo_bit = (vram[tile_start + line]>>(7 - (xpos%8))) & 0x1;
		uint8_t tile_hi_bit = (vram[tile_start + line + 1]>>(7 - (xpos%8))) & 0x1;
		int     pixel_color = (tile_hi_bit<<1) | tile_lo_bit;
		uint8_t color       = translate_color(pixel_color, state.bgrdpalette);

		gpu_canvas_put_pixel(pixel, scanline, color);
	}
}


#ifdef DEBUG_BUILD
/* RENDERS ALL TILES FROM WHOLE TILE MEMORY */
static void gpu_debug(void) {
	uint16_t tile_base = 0x0000; // vram offset
	const uint8_t  tile_size = 16;

	gpu_debug_clear();

	for (uint8_t index_y = 0; index_y < 24; ++index_y) {
		for (uint8_t index_x = 0; index_x < 16; ++index_x) {
			uint16_t tile_start = tile_base + index_y * 16 * tile_size + index_x * tile_size;

			for (uint8_t y = 0; y < 8; ++y) {
				uint8_t line = y * 2;

				for(uint8_t x = 0; x < 8; ++x) {
					uint8_t tile_lo_bit = (vram[tile_start + line] >> (7 - x)) & 0x1;
					uint8_t tile_hi_bit = (vram[tile_start + line + 1] >> (7 - x)) & 0x1;
					int pixel_color = (tile_hi_bit << 1) | tile_lo_bit;
					uint8_t color = color_to_default_palette(pixel_color);
					gpu_debug_put_pixel(index_x * 8 + x, index_y * 8 + y, color, color, color);
				}
			}
		}
	}
	gpu_debug_render();
}
#endif /* debug end */

void gpu_oam_write (uint16_t addr, uint8_t val) {
	oam[addr] = val;
}

uint8_t gpu_oam_read (uint16_t addr) {
	return oam[addr];
}

void gpu_write (uint16_t addr, uint8_t val) {
	vram[addr] = val;
}

uint8_t gpu_read(uint16_t addr) {
	return vram[addr];
}

void gpu_write_reg(uint16_t addr, uint8_t val) {
	switch(addr) {
	case 0xFF40:
		state.lcd_control = val;
		break;
	case 0xFF41:
		state.lcd_stat = val;
		break;
	case 0xFF42:
		state.scrolly = val;
		break;
	case 0xFF43:
		state.scrollx = val;
		break;
	case 0xFF44:
		state.curline = 0;
		break;
	case 0xFF45:
		state.cmpline = val;
		break;
	case 0xFF47:
		parse_colors_from_bit_palette(val, state.bgrdpalette);
		break;
	case 0xFF48:
		parse_colors_from_bit_palette(val, state.palette0);
		break;
	case 0xFF49:
		parse_colors_from_bit_palette(val, state.palette1);
		break;
	case 0xFF4A:
		state.wndposy = val;
		break;
	case 0xFF4B:
		state.wndposx = val;
		break;
	case 0xFF46:
		for (uint8_t index = 0; index <= 0x9F; ++index) {
			oam[index] = cpu_get_dma(val, index);
		}
		break;
	default:
		break;
	}
}

uint8_t gpu_read_reg(uint16_t addr) {
	switch(addr) {
	case 0xFF40:
		return state.lcd_control;
	case 0xFF41:
		return state.lcd_stat;
	case 0xFF42:
		return state.scrolly;
	case 0xFF43:
		return state.scrollx;
	case 0xFF44:
		return state.curline;
	case 0xFF45:
		return state.cmpline;
	case 0xFF4A:
		return state.wndposy;
	case 0xFF4B:
		return state.wndposx;
	case 0xFF47:
	case 0xFF48:
	case 0xFF49:
	case 0xFF46:
		return 0;
	default:
		return 0;
	}
}

static uint8_t inline translate_color (int color, uint8_t *palette) {
	int translated_color = palette[color];
	return color_to_default_palette(translated_color);
}

static uint8_t inline color_to_default_palette (int color) {
	switch (color) {
	case 0:
		return 255;
	case 1:
		return 192;
	case 2:
		return 96;
	case 3:
		default:
			return 0;
	}
}

static void inline parse_colors_from_bit_palette (uint8_t palette, uint8_t *palette_save) {
	palette_save[0] = (palette>>0) & 0x3;
	palette_save[1] = (palette>>2) & 0x3;
	palette_save[2] = (palette>>4) & 0x3;
	palette_save[3] = (palette>>6) & 0x3;
}
