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

	/* PALETTES */
	uint8_t bgrdpal;
	uint8_t obj0pal;
	uint8_t obj1pal;

	/* WINDOW POSITIONS */
	uint8_t wndposy;
	uint8_t wndposx;

	int scanline_counter;
} gpu_state;

static gpu_state state;
static uint8_t   vram[0x2000]; // video ram, 8 kbytes
static uint8_t   oam[0xA0]; // oam ram

static void gpu_render_bg (int scanline);

static void gpu_render_sprites (void);

static uint8_t inline color_to_default_palette (int color);

void gpu_init (void) {
	memset(oam, 0x00, 0xA0);
	state.scanline_counter = 456;

	screen_clear();
	screen_vsync();
}

/* TODO MAKE IT BETTER */
void gpu_step(int cycles) {
	int     mode;
	uint8_t status            = state.lcd_stat;
	int     current_mode      = state.lcd_stat & 3;
	bool    request_interrupt = false;

	if (!(state.lcd_control & 0x80)) {
		screen_clear();
		screen_vsync();
		state.scanline_counter = 456;
		state.curline          = 0;
		status &= 252;
		status &= ~(1 << 0);
		status &= ~(1 << 1);
		state.lcd_stat = status;
		return;
	}

	if (state.curline >= 144) {
		mode              = 1;
		status |= (1 << 0);
		status &= ~(1 << 1);
		request_interrupt = (status & 0x10) ? true : false;
	} else if (state.scanline_counter >= 376) {
		mode              = 2;
		status &= ~(1 << 0);
		status |= (1 << 1);
		request_interrupt = (status & 0x20) ? true : false;
	} else if (state.scanline_counter >= 204) {
		mode = 3;
		status |= (1 << 0);
		status |= (1 << 1);
		if (mode != current_mode) {
			gpu_render_bg(state.curline);
		}
	} else {
		mode              = 0;
		status &= ~(1 << 0);
		status &= ~(1 << 1);
		request_interrupt = (status & 0x8) ? true : false;
		if (mode != current_mode) {
		}
	}

	if (request_interrupt && mode != current_mode) {
		cpu_request_interrupt(1);
	}

	if (state.curline == state.cmpline) {
		status |= (1 << 2);

		if (status & 0x40) {
			cpu_request_interrupt(1);
		}
	} else {
		status &= ~(1<<2);
	}

	state.lcd_stat = status;

	state.scanline_counter -= cycles;

	if (state.scanline_counter <= 0) {
		state.curline++;
		if (state.curline > 153) {
			state.curline = 0;

			gpu_render_sprites();

			screen_vsync();
		}

		state.scanline_counter += 456;

		if (state.curline == SCREEN_HEIGHT) {
			cpu_request_interrupt(0);
		}
	}
}

static void gpu_render_sprites (void) {
	const uint8_t sprite_size = (state.lcd_control & 0x4) ? 2 : 1;

	for (uint8_t sprite = 0; sprite < 40; ++sprite) {
		uint8_t sprite_offset = sprite*4;

		// position for right bottom pixel, so we must subtract 8/16 px for correct rendering
		uint8_t sprite_y = oam[sprite_offset];              // y pos
		uint8_t sprite_x = oam[sprite_offset + 1];          // x pos
		uint8_t sprite_n = oam[sprite_offset + 2];          // number in tile table
		uint8_t sprite_a = oam[sprite_offset + 3];          // attribute bit array



		bool use_first_palette = (sprite_a & 0x10) ? true : false; // TODO ALL LOGIC WITH PALETTES
		bool flip_x            = (sprite_a & 0x20) ? true : false;
		bool flip_y            = (sprite_a & 0x40) ? true : false;
		bool lower_prio        = (sprite_a & 0x80) ? true : false;

		if (sprite_y == 0 || sprite_y >= SCREEN_HEIGHT + 16) {
			continue;
		}

		if (sprite_x == 0 || sprite_x >= SCREEN_WIDTH + 8) {
			continue;
		}

		uint8_t screen_y = sprite_y - 16;
		uint8_t screen_x = sprite_x - 8;

		uint16_t tile_offset = 0x0000 + sprite_n*16;

		for (uint8_t y = 0; y < (8*sprite_size); ++y) {
			for (uint8_t x = 0; x < 8; ++x) {
				uint8_t ypos = !flip_y ? y : (8*sprite_size) - y - 1;
				uint8_t xpos = !flip_x ? x : 8 - x - 1;

				if (screen_y + y > SCREEN_HEIGHT || screen_x + x > SCREEN_WIDTH) {
					continue;
				}

				uint8_t tile_lo_bit = (vram[tile_offset + ypos*2]>>(8 - xpos - 1)) & 0x1;
				uint8_t tile_hi_bit = (vram[tile_offset + ypos*2 + 1]>>(8 - xpos - 1)) & 0x1;

				int pixel_color = (tile_hi_bit<<1) | tile_lo_bit;

				// TODO: check this crap
				if (lower_prio && pixel_color != 255) {
					continue;
				}

				uint8_t color = color_to_default_palette(pixel_color);
				screen_put_pixel(screen_x + xpos, screen_y + ypos, color, color, color);
			}
		}
	}
}

static void gpu_render_bg (int scanline) {
	const uint16_t bg_base   = (state.lcd_control & 0x8) ? 0x1C00 : 0x1800;
	const uint16_t tile_base = (state.lcd_control & 0x10) ? 0x0000 : 0x0800;
	const uint8_t  tile_size = 16;

	uint16_t ypos     = state.scrolly + scanline;
	uint8_t  tile_row = ypos/8;

	if (!(state.lcd_control & 0x1)) {
		screen_clear();
		return;
	}

	for (uint8_t pixel = 0; pixel < 160; ++pixel) {
		uint16_t xpos = pixel + state.scrollx;
		uint8_t tile_col = xpos / 8;

		uint16_t tile_index = vram[bg_base + tile_row * 32 + tile_col];

		if (tile_base == 0x0800) {
			tile_index += 256;
		}

		uint16_t tile_start = tile_base + tile_index * tile_size;
		uint8_t line = (ypos % 8) * 2;

		uint8_t tile_lo_bit = (vram[tile_start + line] >> (7 - (xpos % 8))) & 0x1;
		uint8_t tile_hi_bit = (vram[tile_start + line + 1] >> (7 - (xpos % 8))) & 0x1;
		int pixel_color = (tile_hi_bit << 1) | tile_lo_bit;
		uint8_t color = color_to_default_palette(pixel_color);
		screen_put_pixel(pixel, scanline, color, color, color);
	}
}

void gpu_oam_write(uint16_t addr, uint8_t val) {
	oam[addr] = val;
}

uint8_t gpu_oam_read(uint16_t addr) {
	return oam[addr];
}

void gpu_write(uint16_t addr, uint8_t val) {
	//println("gpu mem write 0x%04x = 0x%02x", addr + 0x8000, val);
	vram[addr] = val;
}

uint8_t gpu_read(uint16_t addr) {
	//println("gpu mem read 0x%02x at addr 0x%04x", vram[addr], addr + 0x8000);
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
		state.bgrdpal = val;
		break;
	case 0xFF48:
		state.obj0pal = val;
		break;
	case 0xFF49:
		state.obj1pal = val;
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
	case 0xFF47:
		return state.bgrdpal;
	case 0xFF48:
		return state.obj0pal;
	case 0xFF49:
		return state.obj1pal;
	case 0xFF4A:
		return state.wndposy;
	case 0xFF4B:
		return state.wndposx;
	case 0xFF46:
		return 0;
	default:
		return 0;
	}
}

static uint8_t inline color_to_default_palette(int color) {
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

