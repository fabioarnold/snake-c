#include <math.h>
#include <stdio.h>
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

int quit = 0;
unsigned int last_tick;
const int DELTA_TICK = 120;
SDL_Window* sdl_window;

void identityM3(float* m);
void translateM3(float* m, float tx, float ty);
void scaleM3(float* m, float sx, float sy);
void rotateM3(float* m, float angle);
void multiplyM3(float* m, float* m0, float* m1);

GLuint program;
GLint transform_loc;
GLint color_loc;
const int draw_detail = 16;
float vertex_data[2 * 2 * (draw_detail + 1)];
GLuint vbos[2];

const int W = 25;
const int H = 15;
const int N = W * H;

typedef enum {
	NORTH, SOUTH, EAST, WEST
} Direction;

typedef struct {
	int x, y;
	Direction dir;
} Segment;

struct Game {
	Segment snake[N];
	int head;
	int tail;
	Direction dir;

	int food_x, food_y;
	int eaten;

	int gameover;
} game;

void resetGame();
void gameover();
void placeFood();
void tickGame();
void drawGame(float alpha);

void resetGame() {
	game.head = 1;
	game.tail = 0;
	game.dir = SOUTH;

	game.snake[game.head] = (Segment){ W / 2, H - 3, SOUTH };
	game.snake[game.tail] = (Segment){ W / 2, H - 2, SOUTH };

	game.gameover = 0;

	placeFood();
	game.eaten = 0;
}

void gameover() {
	game.gameover = 1;
	int len = (game.head + 1 - game.tail + N) % N;
	printf("snake length: %d\n", len);
}

void placeFood() {
	int len = (game.head + 1 - game.tail + N) % N;
	char grid[N] = {}; // mark occupied cells
	for (int i = 0; i < len; i++) {
		Segment* s = &game.snake[(game.tail + i) % N];
		grid[W * s->y + s->x] = 1;
	}
	int f = rand() % (N - len); // choose free cell index
	for (int i = 0; i < N; i++) {
		if (!grid[i]) {
			if (!f) {
				game.food_x = i % W;
				game.food_y = i / W;
				return;
			}
			f--;
		}
	}
}

void tickGame() {
	if (game.gameover) return;

	Segment* head = &game.snake[game.head];
	switch (head->dir) {
		case NORTH: if (game.dir == SOUTH) game.dir = NORTH; break;
		case SOUTH: if (game.dir == NORTH) game.dir = SOUTH; break;
		case EAST: if (game.dir == WEST) game.dir = EAST; break;
		case WEST: if (game.dir == EAST) game.dir = WEST; break;
	}
	if ((game.dir == NORTH && head->y == H - 1) ||
		(game.dir == SOUTH && head->y == 0) ||
		(game.dir == EAST  && head->x == W - 1) ||
		(game.dir == WEST  && head->x == 0)) {
		gameover();
		return;
	}

	game.head = (game.head + 1) % N;
	switch (game.dir) {
		case NORTH: game.snake[game.head] = (Segment){ head->x, head->y + 1, NORTH }; break;
		case SOUTH: game.snake[game.head] = (Segment){ head->x, head->y - 1, SOUTH }; break;
		case EAST:  game.snake[game.head] = (Segment){ head->x + 1, head->y, EAST  }; break;
		case WEST:  game.snake[game.head] = (Segment){ head->x - 1, head->y, WEST  }; break;
	}
	head = &game.snake[game.head];

	for (int i = game.tail; i != game.head; i = (i + 1) % N) {
		if (head->x == game.snake[i].x && head->y == game.snake[i].y) {
			gameover();
			return;
		}
	}

	if (head->x == game.food_x && head->y == game.food_y) {
		game.eaten = 1;
		if (game.head == game.tail) {
			gameover();
			return;
		}
		placeFood();
	} else {
		game.eaten = 0;
		game.tail = (game.tail + 1) % N;
	}
}

void drawGame(float alpha) {
	int width, height;
	SDL_GL_GetDrawableSize(sdl_window, &width, &height);
	glViewport(0, 0, width, height);
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	float scale[9], translation[9], rotation[9], tmp[9], transform[9];

	float a0 = (float)width / height;
	float a1 = (float)W / H;
	float sx = 1.0f, sy = 1.0f;
	if (a0 > a1) sx = a1 / a0;
	else sy = a0 / a1;

	// background
	glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
	glEnableVertexAttribArray(0); // position
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL);
	scaleM3(scale, sx, sy);
	glUniformMatrix3fv(transform_loc, 1, GL_FALSE, scale);
	glUniform3f(color_loc, 0.3f, 0.3f, 0.5f);
	glDrawArrays(GL_LINE_LOOP, 0, 4);

	// sprite types
	glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	glEnableVertexAttribArray(0); // position
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL);
	for (int s = 0; s < 4; s++) {
		for (int i = 0; i <= draw_detail; i++) {
			float angle = (float)i / draw_detail * M_PI;
			vertex_data[2 * i + 0] = cosf(angle);
			vertex_data[2 * i + 1] = sinf(angle);
			vertex_data[2 * (draw_detail + 1 + i) + 0] = -cosf(angle);
			vertex_data[2 * (draw_detail + 1 + i) + 1] = -sinf(angle) - 2.0f;
			if (s == 1) { // head
				vertex_data[2 * i + 1] += 2.0f * alpha - 2.0f;
			} else if (s == 2) { // tail
				vertex_data[2 * (draw_detail + 1 + i) + 1] += 
					game.eaten ? 2.0f : 2.0f * alpha;
			} else if (s == 3) { // food
				vertex_data[2 * (draw_detail + 1 + i) + 1] += 2.0f;
			}
		}
		glBufferSubData(GL_ARRAY_BUFFER, 
			s * sizeof(vertex_data), sizeof(vertex_data), vertex_data);
	}
	int n = 2 * (draw_detail + 1);

	// snake
	scaleM3(scale, sx / W, sy / H);
	glUniform3f(color_loc, 0.3f, 0.7f, 0.1f);
	for (int i = game.tail; ; i = (i + 1) % N) {
		Segment* s = &game.snake[i];
		int stype = i == game.tail ? 2 : i == game.head ? 1 : 0;
		translateM3(translation,
			2.0f * s->x - W + 1.0f, 
			2.0f * s->y - H + 1.0f);
		int rt = s->dir == NORTH ? 0 : s->dir == SOUTH ? 2 : 
				 s->dir == EAST  ? 3 : s->dir == WEST  ? 1 : 0;
		rotateM3(rotation, (float)rt / 2 * M_PI);
		multiplyM3(tmp, translation, scale);
		multiplyM3(transform, rotation, tmp);
		glUniformMatrix3fv(transform_loc, 1, GL_FALSE, transform);

		glDrawArrays(GL_TRIANGLE_FAN, stype * n, n);
		if (i == game.head) break;
	}

	// food
	if (!game.gameover) {
		glUniform3f(color_loc, 1.0f, 0.2f, 0.0f);
		translateM3(translation,
			2.0f * game.food_x - W + 1.0f, 
			2.0f * game.food_y - H + 1.0f);
		multiplyM3(transform, translation, scale);
		glUniformMatrix3fv(transform_loc, 1, GL_FALSE, transform);
		glDrawArrays(GL_TRIANGLE_FAN, 3 * n, n);
	}

	SDL_GL_SwapWindow(sdl_window);
}

void identityM3(float* m) {
	m[0] = 1.0f; m[3] = 0.0f; m[6] = 0.0f;
	m[1] = 0.0f; m[4] = 1.0f; m[7] = 0.0f;
	m[2] = 0.0f; m[5] = 0.0f; m[8] = 1.0f;
}

void translateM3(float* m, float tx, float ty) {
	m[0] = 1.0f; m[3] = 0.0f; m[6] = tx;
	m[1] = 0.0f; m[4] = 1.0f; m[7] = ty;
	m[2] = 0.0f; m[5] = 0.0f; m[8] = 1.0f;
}

void scaleM3(float* m, float sx, float sy) {
	m[0] = sx;   m[3] = 0.0f; m[6] = 0.0f;
	m[1] = 0.0f; m[4] = sy;   m[7] = 0.0f;
	m[2] = 0.0f; m[5] = 0.0f; m[8] = 1.0f;
}

void rotateM3(float* m, float angle) {
	float s = sinf(angle), c = cosf(angle);
	m[0] =  c;   m[3] = -s;   m[6] = 0.0f;
	m[1] =  s;   m[4] =  c;   m[7] = 0.0f;
	m[2] = 0.0f; m[5] = 0.0f; m[8] = 1.0f;
}

void multiplyM3(float* m, float* m0, float* m1) {
	memset(m, 0, 9 * sizeof(float));
	for (int i = 0; i < 3; i++)
	for (int j = 0; j < 3; j++)
	for (int k = 0; k < 3; k++)
		m[3 * j + i] += m0[3 * j + k] * m1[3 * k + i];
}

GLuint makeShader(const char* vert_src, const char* frag_src) {
	GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert_shader, 1, &vert_src, NULL);
	glCompileShader(vert_shader);
	GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag_shader, 1, &frag_src, NULL);
	glCompileShader(frag_shader);
	GLuint program = glCreateProgram();
	glAttachShader(program, vert_shader);
	glAttachShader(program, frag_shader);
	glLinkProgram(program);
	glDetachShader(program, vert_shader);
	glDetachShader(program, frag_shader);
	glDeleteShader(vert_shader);
	glDeleteShader(frag_shader);
	return program;
}

int sdlEventWatch(void* userdata, SDL_Event* sdl_event) {
	if (sdl_event->type == SDL_WINDOWEVENT 
		&& sdl_event->window.event == SDL_WINDOWEVENT_RESIZED) {
		drawGame(1.0f);
		return 0; // handled
	}
	return 1;
}

void mainloop();

int main(int argc, char* argv[]) {
	int video_width = 1024;
	int video_height = 640;

	SDL_Init(SDL_INIT_VIDEO);
#ifdef EMSCRIPTEN
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
		SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	sdl_window = SDL_CreateWindow("Snake", 
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
		video_width, video_height, 
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_GLContext gl_context = SDL_GL_CreateContext(sdl_window);
	SDL_GL_SetSwapInterval(1);

	SDL_AddEventWatch(sdlEventWatch, NULL); // draw game during resize

	program = makeShader(
		"uniform mat3 transform;"
		"attribute vec2 position;"
		"void main() {"
		"	vec3 p = transform * vec3(position, 1.0);"
		"	gl_Position = vec4(p.xy, 0.0, 1.0);"
		"}",

		"#ifdef GL_ES\n"
		"precision mediump float;\n"
		"#endif\n"
		"uniform vec3 color;"
		"void main() {"
		"	gl_FragColor = vec4(color, 1.0);"
		"}"
	);
	glUseProgram(program);
	transform_loc = glGetUniformLocation(program, "transform");
	color_loc = glGetUniformLocation(program, "color");

	float rect_data[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f,
	};
	glGenBuffers(2, vbos);
	glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vertex_data), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect_data), rect_data, GL_STATIC_DRAW);

	resetGame();
	game.gameover = 1;
	last_tick = SDL_GetTicks();

#ifdef EMSCRIPTEN
	emscripten_set_main_loop(mainloop, 0, 1);
#else
	while (!quit) mainloop();
#endif

	SDL_Quit();

	return 0;
}

void mainloop() {
	SDL_Event sdl_event;
	while (SDL_PollEvent(&sdl_event)) {
		switch (sdl_event.type) {
			case SDL_QUIT:
				quit = 1;
				break;
			case SDL_KEYDOWN:
				if (sdl_event.key.keysym.sym == SDLK_ESCAPE) quit = 1;
				switch (sdl_event.key.keysym.sym) {
					case SDLK_UP: game.dir = NORTH; break;
					case SDLK_DOWN: game.dir = SOUTH; break;
					case SDLK_RIGHT: game.dir = EAST; break;
					case SDLK_LEFT: game.dir = WEST; break;
				}
				if (SDL_GetTicks() - last_tick > 500 && game.gameover) {
					resetGame();
					last_tick = SDL_GetTicks();
					tickGame();
				}
				break;
#ifdef EMSCRIPTEN // Touch controls
			case SDL_FINGERDOWN: {
				float x = sdl_event.tfinger.x;
				float y = sdl_event.tfinger.y;
				if (x < 0.5f) {
					if (y < x) {
						game.dir = NORTH;
					} else if (1.0f - y < x) {
						game.dir = SOUTH;
					} else {
						game.dir = WEST;
					}
				} else {
					if (y > x) {
						game.dir = SOUTH;
					} else if (1.0f - y > x) {
						game.dir = NORTH;
					} else {
						game.dir = EAST;
					}
				}
				if (SDL_GetTicks() - last_tick > 500 && game.gameover) {
					resetGame();
					last_tick = SDL_GetTicks();
					tickGame();
				}
			} break;
#endif
		}
	}

	float alpha = 1.0f;
	if (!game.gameover) {
		unsigned int ticks = SDL_GetTicks();
		while (ticks >= last_tick + DELTA_TICK) {
			tickGame();
			last_tick += DELTA_TICK;
		}
		alpha = (float)((int)ticks - (int)last_tick) / DELTA_TICK;
	}
	if (game.gameover) alpha = 1.0f;
	drawGame(alpha);
}