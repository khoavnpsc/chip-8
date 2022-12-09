#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <Windows.h>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;

uint8_t memory[4096] = {}; // The interpreter's memories.
uint8_t v[16] = {}; // The interpreter's v registers.
uint16_t index = 0; // The interpreter's index register.
uint8_t delay_timer = 0; // The delay timer.
uint8_t sound_timer = 0; // The sound timer.
uint32_t display[64 * 32] = {}; // The display.
uint16_t pc = 0x200; // The interpreter's program counter. The default value is 0x200 since the program is stored in memory starting at address 0x200.
uint16_t stack[16] = {}; // The interpreter's stack.
uint8_t sp = 0; // The interpreter's stack pointer.
uint8_t keypad[16] = {}; // The interpreter's keypad.
uint8_t fontset[80] = { // The font from 0 to F to be displayed on the screen.
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void Platform_Init(char* title) {
	SDL_Init(SDL_INIT_VIDEO);
	window = SDL_CreateWindow(title, 100, 50, 640 * 2, 320 * 2, SDL_WINDOW_SHOWN);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);
}

void Platform_Update() {
	SDL_UpdateTexture(texture, 0, display, sizeof(display[0]) * 64);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, 0, 0);
	SDL_RenderPresent(renderer);
}

void Platform_Destroy() {
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

int main(int argc, char** argv) {
	// Read the argument.
	if (argc != 2) {
		puts("Usage: chip-8-c++.exe rom_name");
		exit(0);
	}

	// Platform initialization.
	Platform_Init(argv[1]);

	// Load the rom to the memory.
	FILE* rom_ptr;
	int rom_len;
	errno_t open_err = fopen_s(&rom_ptr, argv[1], "rb");
	if (!open_err) {
		fseek(rom_ptr, 0, SEEK_END); // Move the pointer to the end of the file and rewind it to get the file's length.
		rom_len = ftell(rom_ptr);
		rewind(rom_ptr);
		fread(&memory[0x200], rom_len, 1, rom_ptr); // Load the rom at the memory 0x200.
		fclose(rom_ptr); // Close the file.
	}
	else {
		puts("Rom path is not found!");
		exit(0);
	}

	// Load the font. For some reason, it's become popular to put it between 0x50 and 0x9F.
	for (int i = 0; i < 0x50; i++) {
		memory[0x50 + i] = fontset[i];
	}

	// At this point, everything we need are already in the memory.
	uint16_t opcode;
	while (true) {
		// Fetching stage.
		if (pc & 1) {
			puts("Invalid address");
			exit(0);
		}
		opcode = memory[pc] << 8 | memory[pc + 1]; // Fetch the opcode.
		//printf("Ins: %x\n", opcode);
		pc += 2; // Increase the program counter by one instruction.
		uint8_t n = opcode & 0x000F;
		uint16_t nnn = opcode & 0x0FFF;
		uint8_t kk = opcode & 0x00FF;
		uint8_t x = (opcode & 0x0F00) >> 8;
		uint8_t y = (opcode & 0x00F0) >> 4;
		uint8_t x_coor;
		uint8_t y_coor;
		uint16_t temp;

		// Decode stage
		switch (opcode & 0xF000) {
		case 0x0000:
			switch (opcode) {
			case 0x00E0: // CLS
				memset(display, 0, 64 * 32);
				break;
			case 0x00EE: // RET
				--sp;
				pc = stack[sp];
				break;
			}
			break;

		case 0x1000: // 1nnn - JP addr
			pc = nnn;
			break;

		case 0x2000: // 2nnn - CALL addr
			stack[sp] = pc;
			++sp;
			pc = nnn;
			break;

		case 0x3000: // 3xkk - SE Vx, byte
			if (v[x] == kk)
				pc += 2;
			break;

		case 0x4000: // 4xkk - SNE Vx, byte
			if (v[x] != kk)
				pc += 2;
			break;

		case 0x5000: // 5xy0 - SE Vx, Vy
			if (v[x] == v[y])
				pc += 2;
			break;

		case 0x6000: // 6xkk - LD Vx, byte
			v[x] = kk;
			break;

		case 0x7000: // 7xkk - ADD Vx, byte
			v[x] += kk;
			break;

		case 0x8000:
			switch (n) {
			case 0: // 8xy0 - LD Vx, Vy
				v[x] = v[y];
				break;

			case 1: // 8xy1 - OR Vx, Vy
				v[x] |= v[y];
				break;

			case 2: // 8xy2 - AND Vx, Vy
				v[x] &= v[y];
				break;

			case 3: // 8xy3 - XOR Vx, Vy
				v[x] ^= v[y];
				break;

			case 4: // 8xy4 - ADD Vx, Vy
				v[x] += v[y];
				temp = v[x] + v[y];
				v[0xf] = (temp == v[x] ? 0 : 1);
				break;
				
			case 5: // 8xy5 - SUB Vx, Vy
				v[x] -= v[y];
				v[0xf] = (v[x] > v[y] ? 1 : 0);
				break;

			case 6: // 8xy6 - SHR Vx
				v[0xf] = v[x] & 1;
				v[x] >>= 1;
				break;

			case 7: // 8xy7 - SUBN Vx, Vy
				v[x] = v[y] - v[x];
				v[0xf] = (v[y] > v[x] ? 1 : 0);
				break;

			case 0xE: // 8xyE - SHL Vx {, Vy}
				v[0xf] = (v[x] & 0x80) >> 7;
				v[x] <<= 1;
				break;
			}
			break;
		case 0x9000: // 9xy0 - SNE Vx, Vy
			if (v[x] != v[y])
				pc += 2;
			break;

		case 0xA000: // Annn - LD I, addr
			index = nnn;
			break;

		case 0xD000: // Dxyn - DRW Vx, Vy, nibble
			// Wrap if out of bound.
			x_coor = v[x] % 64;
			y_coor = v[y] % 32;

			// Set VF to 0.
			v[0xf] = 0;

			// For n rows.
			for (int row = 0; row < n; row++) {
				// Get the Nth byte of sprite data, counting from the memory address in the I register (I is not incremented).
				uint8_t sprite_byte = memory[index + row];
				// For each of the 8 pixels/bits in this sprite row:
				for (int col = 0; col < 8; col++) {
					uint8_t sprite_bit = sprite_byte & (0x80 >> col);
					uint32_t* display_bit = &display[(y_coor + row) * 64 + (x_coor + col)];
					// If the sprite pixel is on.
					if (sprite_bit) {
						// And the screen pixel is also on.
						if (*display_bit == 0xffffffff) {
							// Set VF to 1.
							v[0xf] = 1;
						}
						// Xor the bit.
						*display_bit ^= 0xffffffff;
					}
				}
			}
			break;

		// Key instructions from here
		case 0xE000:
			switch (opcode & 0x00FF) {
			case 0x009E: // Ex9E - SKP Vx
				if (keypad[v[x]])
					pc += 2;
				break;

			case 0x00A1: // ExA1 - SKNP Vx
				if (!keypad[v[x]])
					pc += 2;
				break;
			}
			break;

		case 0xF000:
			switch (opcode & 0x00FF) {
			case 0x0007: // Fx07 - LD Vx, DT
				v[x] = delay_timer;
				break;

			case 0x000A: // Fx0A - LD Vx, K
				// TODO
				break;

			case 0x0015: // Fx15 - LD DT, Vx
				delay_timer = v[x];
				break;

			case 0x0018: // Fx18 - LD ST, Vx
				sound_timer = v[x];
				break;

			case 0x001E: // Fx1E - ADD I, Vx
				index += v[x];
				break;

			case 0x0029: // Fx29 - LD F, Vx
				index = 0x50 + (5 * v[x]);
				break;

			case 0x0033: // Fx33 - LD B, Vx
				temp = v[x];
				memory[index + 2] = temp % 10;
				temp /= 10;
				memory[index + 1] = temp % 10;
				temp /= 10;
				memory[index] = temp % 10;
				break;

			case 0x0055: // Fx55 - LD [I], Vx
				for (uint8_t i = 0; i <= x; ++i) {
					memory[index + i] = v[i];
				}
				break;

			case 0x0065: // Fx65 - LD Vx, [I]
				for (uint8_t i = 0; i <= x; ++i) {
					v[i] = memory[index + i];
				}
				break;
			}
			break;
		}
		// Update the platform.
		Platform_Update();
	}
	// Destroy the platform.
	Platform_Destroy();
}