#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include "pokedex.h"

#define GRID_COLS 3
#define GRID_ROWS 2
#define FOOTER_H 100
#define FONT_SIZE 74

int screen_w = 720;
int screen_h = 720;
int card_w = 240;
int card_h = 330;
SDL_GameController *controller = NULL;

typedef struct {
    const char *name;
    SDL_Color color;
} TypeColor;

TypeColor type_colors[] = {
    {"Normal",   {168, 168, 120, 255}}, {"Fire",     {240, 128, 48,  255}},
    {"Water",    {104, 144, 240, 255}}, {"Electric", {248, 208, 48,  255}},
    {"Grass",    {120, 200, 80,  255}}, {"Ice",      {152, 216, 216, 255}},
    {"Fighting", {192, 48,  40,  255}}, {"Poison",   {160, 64,  160, 255}},
    {"Ground",   {224, 192, 104, 255}}, {"Flying",   {168, 144, 240, 255}},
    {"Psychic",  {248, 88,  136, 255}}, {"Bug",      {168, 184, 32,  255}},
    {"Rock",     {184, 160, 56,  255}}, {"Ghost",    {112, 88,  152, 255}},
    {"Dragon",   {112, 56,  248, 255}}, {"Dark",     {112, 88,  72,  255}},
    {"Steel",    {184, 184, 208, 255}}, {"Fairy",    {238, 153, 172, 255}},
    {NULL,       {104, 160, 144, 255}}  // Default
};

typedef struct {
    SDL_Texture *tex;
    SDL_Rect bbox; // The active non-transparent area of the sprite
} SpriteRef;

SpriteRef sprites[800];
SDL_Texture *ball_tex;
TTF_Font *font;

// Helper to find the bounding box of non-transparent pixels in a surface
void find_sprite_bbox(SDL_Surface *surf, SDL_Rect *bbox) {
    if (!surf) return;
    int min_x = surf->w, min_y = surf->h;
    int max_x = 0, max_y = 0;
    bool found = false;

    SDL_LockSurface(surf);
    Uint32 *pixels = (Uint32 *)surf->pixels;
    int pitch = surf->pitch / 4;

    for (int y = 0; y < surf->h; y++) {
        for (int x = 0; x < surf->w; x++) {
            Uint32 pixel = pixels[y * pitch + x];
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixel, surf->format, &r, &g, &b, &a);
            if (a > 0) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
                found = true;
            }
        }
    }
    SDL_UnlockSurface(surf);

    if (found) {
        bbox->x = min_x;
        bbox->y = min_y;
        bbox->w = max_x - min_x + 1;
        bbox->h = max_y - min_y + 1;
    } else {
        bbox->x = 0; bbox->y = 0; bbox->w = surf->w; bbox->h = surf->h;
    }
}

void draw_type_tag(SDL_Renderer *renderer, const char *type, int x, int y, int w, int h) {
    if (!type || type[0] == '\0') return;
    SDL_Color color = type_colors[18].color; // Default
    for (int i = 0; i < 18; i++) {
        if (strcasecmp(type, type_colors[i].name) == 0) {
            color = type_colors[i].color;
            break;
        }
    }

    SDL_Rect r = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 40, 30, 20, 100); // Dark brown semi-trans
    SDL_RenderDrawRect(renderer, &r);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, type, white);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        // Scale text to fit tag
        int tw = surf->w;
        int th = surf->h;
        if (tw > w - 20) { float s = (float)(w-20)/tw; tw *= s; th *= s; }
        if (th > h - 10) { float s = (float)(h-10)/th; tw *= s; th *= s; }
        
        SDL_Rect tr = {x + (w - tw)/2, y + (h - th)/2, tw, th};
        SDL_RenderCopy(renderer, tex, NULL, &tr);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }
}

void render_card(SDL_Renderer *renderer, Pokedex *dex, int i, int x, int y, bool selected) {
    Pokemon *p = &dex->pokemon[i];
    
    // 1. Overall Card Background (Premium Cream / Parchment)
    SDL_Rect card_rect = {x + 10, y + 10, card_w - 20, card_h - 20};
    if (selected) {
        SDL_SetRenderDrawColor(renderer, 255, 250, 200, 255); // Brighter cream
    } else {
        SDL_SetRenderDrawColor(renderer, 245, 242, 215, 255); // Classic cream
    }
    SDL_RenderFillRect(renderer, &card_rect);
    
    // DOUBLE LINE BORDER (Card)
    SDL_SetRenderDrawColor(renderer, 80, 70, 60, 255); // Dark brown
    SDL_RenderDrawRect(renderer, &card_rect);
    SDL_Rect inner_border = {card_rect.x + 3, card_rect.y + 3, card_rect.w - 6, card_rect.h - 6};
    SDL_RenderDrawRect(renderer, &inner_border);

    // 2. Name Header Zone (Top 12%)
    int header_h = card_rect.h * 0.12;
    SDL_Rect header_rect = {card_rect.x + 8, card_rect.y + 8, card_rect.w - 16, header_h};
    SDL_SetRenderDrawColor(renderer, 235, 230, 190, 255);
    SDL_RenderFillRect(renderer, &header_rect);
    
    // DOUBLE LINE BORDER (Header)
    SDL_SetRenderDrawColor(renderer, 100, 90, 80, 255);
    SDL_RenderDrawRect(renderer, &header_rect);
    SDL_Rect h_inner = {header_rect.x + 2, header_rect.y + 2, header_rect.w - 4, header_rect.h - 4};
    SDL_RenderDrawRect(renderer, &h_inner);
    
    SDL_Color brown = {60, 40, 30, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, p->name, brown);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        int tw = surf->w, th = surf->h;
        if (tw > header_rect.w - 20) { float s = (float)(header_rect.w-20)/tw; tw *= s; th *= s; }
        if (th > header_rect.h - 10) { float s = (float)(header_rect.h-10)/th; tw *= s; th *= s; }
        SDL_Rect tr = {header_rect.x + (header_rect.w - tw)/2, header_rect.y + (header_rect.h - th)/2, tw, th};
        SDL_RenderCopy(renderer, tex, NULL, &tr);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }

    // 3. Image Zone (Middle 63%)
    int img_zone_y = header_rect.y + header_rect.h + 8;
    int img_zone_h = card_rect.h * 0.63;
    SDL_Rect img_rect = {card_rect.x + 8, img_zone_y, card_rect.w - 16, img_zone_h};
    SDL_SetRenderDrawColor(renderer, 255, 255, 245, 255);
    SDL_RenderFillRect(renderer, &img_rect);
    
    // DOUBLE LINE BORDER (Image)
    SDL_SetRenderDrawColor(renderer, 120, 110, 100, 255);
    SDL_RenderDrawRect(renderer, &img_rect);
    SDL_Rect img_inner = {img_rect.x + 2, img_rect.y + 2, img_rect.w - 4, img_rect.h - 4};
    SDL_RenderDrawRect(renderer, &img_inner);

    if (sprites[i].tex) {
        SDL_Rect src = sprites[i].bbox;
        int sh = img_rect.h - 15;
        float aspect = (float)src.w / src.h;
        int sw = (int)(sh * aspect);
        if (sw > img_rect.w - 15) {
            sw = img_rect.w - 15;
            sh = (int)(sw / aspect);
        }
        SDL_Rect r = {img_rect.x + (img_rect.w - sw)/2, img_rect.y + (img_rect.h - sh)/2, sw, sh};
        SDL_RenderCopy(renderer, sprites[i].tex, &src, &r);
    }

    // 4. Divider Bar (Info Line - 10%)
    int info_y = img_rect.y + img_rect.h + 8;
    int info_h = card_rect.h * 0.10;
    SDL_Rect info_rect = {card_rect.x + 8, info_y, card_rect.w - 16, info_h};
    SDL_SetRenderDrawColor(renderer, 235, 230, 200, 255);
    SDL_RenderFillRect(renderer, &info_rect);

    char idbuf[16];
    sprintf(idbuf, "#%03d", p->dex_id);
    surf = TTF_RenderUTF8_Blended(font, idbuf, brown);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        int th = info_rect.h - 10;
        float s = (float)th / surf->h;
        int tw = (int)(surf->w * s);
        SDL_Rect tr = {info_rect.x + 10, info_rect.y + (info_rect.h - th)/2, tw, th};
        SDL_RenderCopy(renderer, tex, NULL, &tr);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }

    if (p->caught && ball_tex) {
        int icon_s = info_rect.h - 6;
        SDL_Rect r = {info_rect.x + info_rect.w - icon_s - 10, info_rect.y + 3, icon_s, icon_s};
        SDL_RenderCopy(renderer, ball_tex, NULL, &r);
    } else {
        SDL_Color red = {180, 40, 40, 255}; // Soft red
        surf = TTF_RenderUTF8_Blended(font, "X", red);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            int th = info_rect.h - 6;
            float s = (float)th / surf->h;
            int tw = (int)(surf->w * s);
            SDL_Rect tr = {info_rect.x + info_rect.w - tw - 12, info_rect.y + (info_rect.h - th)/2, tw, th};
            SDL_RenderCopy(renderer, tex, NULL, &tr);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }
    }

    // 5. Types Footer Zone
    int types_y = info_rect.y + info_rect.h + 8;
    int types_h = (card_rect.y + card_rect.h) - types_y - 8;
    if (types_h < 30) types_h = 40;

    int tw = (card_rect.w - 24) / 2;
    if (!p->type2[0]) tw = card_rect.w - 16;

    if (p->type1[0]) {
        draw_type_tag(renderer, p->type1, card_rect.x + 8, types_y, tw, types_h);
    }
    if (p->type2[0]) {
        draw_type_tag(renderer, p->type2, card_rect.x + tw + 16, types_y, tw, types_h);
    }
}

void render_footer(SDL_Renderer *renderer) {
    SDL_Rect r = {0, screen_h - FOOTER_H, screen_w, FOOTER_H};
    SDL_SetRenderDrawColor(renderer, 50, 40, 30, 255); // Darker wood/brown
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 100, 80, 60, 255); // Accent line
    SDL_RenderDrawLine(renderer, 0, screen_h - FOOTER_H, screen_w, screen_h - FOOTER_H);

    SDL_Color gray = {200, 180, 160, 255}; // Light parchment-gray
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, "[A] Caught Toggle     [B] Back to Menu", gray);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        float scale = (float)(FOOTER_H - 15) / surf->h;
        SDL_Rect tr = {20, screen_h - FOOTER_H + 7, (int)(surf->w * scale), FOOTER_H - 15};
        SDL_RenderCopy(renderer, tex, NULL, &tr);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }
}

int main(int argc, char *argv[]) {
    // Redirect stderr to a file so users can check for crashes without a terminal
    freopen("error.log", "w", stderr);
    setvbuf(stderr, NULL, _IOLBF, 0); // Line buffered

    if (argc < 3) {
        printf("Usage: %s <tsv_path> <caught_path>\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); 

    // Explicitly open the first available game controller
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) {
                fprintf(stderr, "DEBUG: Opened GameController 0: %s\n", SDL_GameControllerName(controller));
                break;
            }
        }
    }
    IMG_Init(IMG_INIT_PNG);
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Pokedex", 0, 0, screen_w, screen_h, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }

    // Detect actual screen dimensions for scaling
    SDL_GetRendererOutputSize(renderer, &screen_w, &screen_h);
    card_w = screen_w / GRID_COLS;
    card_h = (screen_h - FOOTER_H) / GRID_ROWS;
    fprintf(stderr, "Detected Screen: %dx%d -> Card Size: %dx%d\n", screen_w, screen_h, card_w, card_h);

    printf("Loading Pokedex:\n  TSV: %s\n  CAUGHT: %s\n", argv[1], argv[2]);
    Pokedex dex;
    if (load_pokedex(argv[1], argv[2], &dex) != 0) {
        fprintf(stderr, "Error loading pokedex data from %s\n", argv[1]);
        return 1;
    }

    font = TTF_OpenFont("data/font.ttf", FONT_SIZE);
    if (!font) font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", FONT_SIZE);
    
    if (!font) {
        fprintf(stderr, "Error: Could not load font. Please ensure data/font.ttf exists.\n");
        return 1;
    }

    // Load static assets
    char path[256];
    sprintf(path, "data/icons/pokeball.png");
    ball_tex = IMG_LoadTexture(renderer, path);

    // Load Sprites
    // Load Sprites with Autocropping
    for (int i = 0; i < dex.count; i++) {
        sprintf(path, "data/sprites/%03d.png", dex.pokemon[i].dex_id);
        SDL_Surface *surf = IMG_Load(path);
        if (surf) {
            find_sprite_bbox(surf, &sprites[i].bbox);
            sprites[i].tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
        } else {
            sprites[i].tex = NULL;
        }
    }

    int selected = 0;
    int scroll = 0;
    bool running = true;
    SDL_Event ev;

    Uint32 last_input = 0;
    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            
            // Controller Events
            if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                fprintf(stderr, "DEBUG: ControllerBtn Down: %d\n", ev.cbutton.button);
                if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                    toggle_caught(&dex, selected);
                    save_caught_status(argv[2], &dex);
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                    running = false;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    if (selected >= GRID_COLS) selected -= GRID_COLS;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    if (selected < dex.count - GRID_COLS) selected += GRID_COLS;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                    if (selected > 0) selected--;
                } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    if (selected < dex.count - 1) selected++;
                }
            }
            // For PC testing, we use GetKeyboardState below for navigation.
            // But we can keep exit/toggle here if we want, or move them too.
        }

        // --- CONSOLIDATED INPUT (Hardware Fallback) ---
        // This handles both PC keyboard and some handheld buttons mapped to keys.
        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (SDL_GetTicks() - last_input > 180) { // Slight increase to debounce
            bool moved = false;
            if (state[SDL_SCANCODE_UP])    { if (selected >= GRID_COLS) selected -= GRID_COLS; moved = true; }
            if (state[SDL_SCANCODE_DOWN])  { if (selected < dex.count - GRID_COLS) selected += GRID_COLS; moved = true; }
            if (state[SDL_SCANCODE_LEFT])  { if (selected > 0) selected--; moved = true; }
            if (state[SDL_SCANCODE_RIGHT]) { if (selected < dex.count - 1) selected++; moved = true; }
            
            if (state[SDL_SCANCODE_RETURN] || state[SDL_SCANCODE_SPACE] || state[SDL_SCANCODE_Z]) {
                toggle_caught(&dex, selected);
                save_caught_status(argv[2], &dex);
                moved = true;
            }
            if (state[SDL_SCANCODE_ESCAPE] || state[SDL_SCANCODE_BACKSPACE] || state[SDL_SCANCODE_X]) {
                running = false;
                moved = true;
            }
            if (moved) last_input = SDL_GetTicks();
        }

        // Grid Auto-scroll (by row)
        int sel_row = selected / GRID_COLS;
        if (sel_row < scroll) scroll = sel_row;
        if (sel_row >= scroll + GRID_ROWS) scroll = sel_row - GRID_ROWS + 1;

        // Background color (Darker parchment / brown)
        SDL_SetRenderDrawColor(renderer, 65, 55, 45, 255); 
        SDL_RenderClear(renderer);

        // Render visible grid cards
        for (int r = 0; r < GRID_ROWS + 1; r++) {
            int row_idx = scroll + r;
            if (row_idx * GRID_COLS >= dex.count) break;
            
            for (int c = 0; c < GRID_COLS; c++) {
                int i = row_idx * GRID_COLS + c;
                if (i >= dex.count) break;
                
                render_card(renderer, &dex, i, c * card_w, r * card_h, i == selected);
            }
        }

        render_footer(renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // Cleanup
    save_caught_status(argv[2], &dex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
