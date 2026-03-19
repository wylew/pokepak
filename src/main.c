#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "pokedex.h"

#define GRID_COLS 3
#define GRID_ROWS 2
#define FOOTER_H 100
#define FONT_SIZE 74

int screen_w = 1024;
int screen_h = 768;
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

typedef enum {
    MODE_GAME_SELECT,
    MODE_POKEDEX
} AppMode;

typedef struct {
    char slug[MAX_STR];      // Directory name
    char name[MAX_STR];      // From game.conf
    char tsv_path[512];
    char caught_path[512];
    int total;
    int caught;
} GameInfo;

GameInfo games[64];
int game_count = 0;
AppMode app_mode = MODE_GAME_SELECT;
int selected_game_idx = 0;

Pokedex main_dex;
Pokedex temp_dex; // For find_games()

// Function Prototypes
void find_games();
void unload_pokedex_assets();
void find_sprite_bbox(SDL_Surface *surf, SDL_Rect *bbox);
void draw_type_tag(SDL_Renderer *renderer, const char *type, int x, int y, int w, int h);
void render_game_select(SDL_Renderer *renderer);
void render_footer(SDL_Renderer *renderer);
void sanitize_pokemon_name(const char *src, char *dest, bool upper);
void load_sprite(SDL_Renderer *renderer, Pokemon *p, SpriteRef *out);

// Scan data/games/ for subdirectories and parse game.conf
void find_games() {
    DIR *d = opendir("data/games");
    if (!d) return;

    struct dirent *dir;
    game_count = 0;
    while ((dir = readdir(d)) != NULL && game_count < 64) {
        if (dir->d_name[0] == '.') continue;
        
        char conf_path[512];
        sprintf(conf_path, "data/games/%s/game.conf", dir->d_name);
        
        FILE *f = fopen(conf_path, "r");
        if (f) {
            strncpy(games[game_count].slug, dir->d_name, MAX_STR - 1);
            games[game_count].slug[MAX_STR - 1] = 0;
            snprintf(games[game_count].tsv_path, 512, "data/games/%s/pokemon.tsv", dir->d_name);
            snprintf(games[game_count].caught_path, 512, "data/games/%s/caught.txt", dir->d_name);
            
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "name=", 5) == 0) {
                    char *val = line + 5;
                    val[strcspn(val, "\r\n")] = 0;
                    strncpy(games[game_count].name, val, MAX_STR - 1);
                    games[game_count].name[MAX_STR - 1] = 0;
                }
            }
            fclose(f);

            // Compute counts
            if (load_pokedex(games[game_count].tsv_path, games[game_count].caught_path, &temp_dex) == 0) {
                games[game_count].total = temp_dex.count;
                games[game_count].caught = 0;
                for (int i = 0; i < temp_dex.count; i++) {
                    if (temp_dex.pokemon[i].caught) games[game_count].caught++;
                }
            }
            game_count++;
        }
    }
    closedir(d);
    fprintf(stderr, "DEBUG: find_games() done. Detected %d games.\n", game_count); fflush(stderr);
}

void sanitize_pokemon_name(const char *src, char *dest, bool upper) {
    int j = 0;
    for (int i = 0; src[i]; i++) {
        char c = src[i];
        if (upper) {
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        } else {
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        }
        
        bool ok = false;
        if (upper) ok = ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
        else ok = ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));

        if (ok) {
            dest[j++] = c;
        } else {
            if (j > 0 && dest[j-1] != '-') dest[j++] = '-';
        }
    }
    if (j > 0 && dest[j-1] == '-') j--;
    dest[j] = 0;
}

void load_sprite(SDL_Renderer *renderer, Pokemon *p, SpriteRef *out) {
    char path[512];
    char sname[128];
    SDL_Surface *surf = NULL;

    // 1. Try lowercase .png
    sanitize_pokemon_name(p->name, sname, false);
    sprintf(path, "data/sprites/%s.png", sname);
    surf = IMG_Load(path);

    // 2. Try uppercase .PNG
    if (!surf) {
        sanitize_pokemon_name(p->name, sname, true);
        sprintf(path, "data/sprites/%s.PNG", sname);
        surf = IMG_Load(path);
    }

    // 3. Try uppercase .png
    if (!surf) {
        sprintf(path, "data/sprites/%s.png", sname);
        surf = IMG_Load(path);
    }

    // 4. Try lowercase .PNG
    if (!surf) {
        sanitize_pokemon_name(p->name, sname, false);
        sprintf(path, "data/sprites/%s.PNG", sname);
        surf = IMG_Load(path);
    }

    // 5. Fallback to ID
    if (!surf) {
        sprintf(path, "data/sprites/%03d.png", p->dex_id);
        surf = IMG_Load(path);
    }

    if (surf) {
        find_sprite_bbox(surf, &out->bbox);
        out->tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
    }
}

void unload_pokedex_assets() {
    for (int i = 0; i < 800; i++) {
        if (sprites[i].tex) {
            SDL_DestroyTexture(sprites[i].tex);
            sprites[i].tex = NULL;
        }
    }
}

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
    if (i < 0 || i >= dex->count) return;

    // Lazy Loading: Load sprite only when needed
    if (!sprites[i].tex) {
        load_sprite(renderer, &dex->pokemon[i], &sprites[i]);
    }

    Pokemon *p = &dex->pokemon[i];
    
    // 1. Overall Card Background (Premium Cream / Parchment)
    SDL_Rect card_rect = {x + 10, y + 10, card_w - 20, card_h - 20};
    if (selected) {
        SDL_SetRenderDrawColor(renderer, 255, 250, 200, 255); // Brighter cream
    } else {
        SDL_SetRenderDrawColor(renderer, 245, 242, 215, 255); // Classic cream
    }
    SDL_RenderFillRect(renderer, &card_rect);
    
    // Selection Highlight Frame (Slate Blue - EXTRA THICK)
    if (selected) {
        SDL_SetRenderDrawColor(renderer, 70, 130, 255, 255);
        for(int t=0; t<4; t++) {
            SDL_Rect h = {card_rect.x - 6 + t, card_rect.y - 6 + t, card_rect.w + 12 - (t*2), card_rect.h + 12 - (t*2)};
            SDL_RenderDrawRect(renderer, &h);
        }
    }

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
    if (!font) return; // CRASH PREVENTION
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
    sprintf(idbuf, "%03d", p->dex_id);
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

void render_game_select(SDL_Renderer *renderer) {
    static bool logged = false; if (!logged) { fprintf(stderr, "DEBUG: render_game_select() called.\n"); fflush(stderr); logged = true; }
    int item_h = 100;
    int spacing = 20;
    int list_w = screen_w * 0.85;
    int total_menu_h = game_count * (item_h + spacing) - spacing;
    int start_y = (screen_h - total_menu_h) / 2;
    if (start_y < 120) start_y = 120;

    for (int i = 0; i < game_count; i++) {
        SDL_Rect r = {(screen_w - list_w) / 2, start_y + i * (item_h + spacing), list_w, item_h};
        bool sel = (i == selected_game_idx);

        // Background
        if (sel) SDL_SetRenderDrawColor(renderer, 255, 250, 210, 255);
        else SDL_SetRenderDrawColor(renderer, 245, 242, 220, 255);
        SDL_RenderFillRect(renderer, &r);

        // Selection Highlight (EXTRA THICK)
        if (sel) {
            SDL_SetRenderDrawColor(renderer, 70, 130, 255, 255);
            for(int t=0; t<4; t++) {
                SDL_Rect h = {r.x - 6 + t, r.y - 6 + t, r.w + 12 - (t*2), r.h + 12 - (t*2)};
                SDL_RenderDrawRect(renderer, &h);
            }
        }

        // Double Border
        SDL_SetRenderDrawColor(renderer, 80, 70, 60, 255);
        SDL_RenderDrawRect(renderer, &r);
        SDL_Rect ir = {r.x + 3, r.y + 3, r.w - 6, r.h - 6};
        SDL_RenderDrawRect(renderer, &ir);

        // Text
        SDL_Color brown = {60, 40, 30, 255};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, games[i].name, brown);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            float s = (float)(item_h - 40) / surf->h;
            if (s > 1.0f) s = 1.0f; // CAPP FONT SCALING
            int tw = (int)(surf->w * s), th = (int)(surf->h * s);
            if (tw > list_w * 0.6) { float s2 = (list_w * 0.6) / tw; tw *= s2; th *= s2; }
            SDL_Rect tr = {r.x + 20, r.y + (item_h - th)/2, tw, th};
            SDL_RenderCopy(renderer, tex, NULL, &tr);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }

        char pbuf[32];
        sprintf(pbuf, "%d / %d", games[i].caught, games[i].total);
        surf = TTF_RenderUTF8_Blended(font, pbuf, brown);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            float s = (float)(item_h - 60) / surf->h;
            int tw = (int)(surf->w * s), th = (item_h - 60);
            SDL_Rect tr = {r.x + r.w - tw - 30, r.y + (item_h - th) / 2, tw, th};
            SDL_RenderCopy(renderer, tex, NULL, &tr);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }
    }

    render_footer(renderer);
}

void render_footer(SDL_Renderer *renderer) {
    SDL_Rect r = {0, screen_h - FOOTER_H, screen_w, FOOTER_H};
    SDL_SetRenderDrawColor(renderer, 50, 40, 30, 255); // Darker wood/brown
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 100, 80, 60, 255); // Accent line
    SDL_RenderDrawLine(renderer, 0, screen_h - FOOTER_H, screen_w, screen_h - FOOTER_H);

    SDL_Color gray = {200, 180, 160, 255}; // Light parchment-gray
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, "[A] Select/Toggle     [B] Back", gray);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            float scale = (float)(FOOTER_H - 18) / surf->h;
            int tw = (int)(surf->w * scale);
            if (tw > screen_w - 40) {
                scale = (float)(screen_w - 40) / surf->w;
                tw = screen_w - 40;
            }
            int th = (int)(surf->h * scale);
            SDL_Rect tr = {(screen_w - tw)/2, screen_h - FOOTER_H + (FOOTER_H - th)/2, tw, th};
            SDL_RenderCopy(renderer, tex, NULL, &tr);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(surf);
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "DEBUG: App Started. Pokedex Init.\n");
    fprintf(stderr, "DEBUG: ENV DISPLAY=%s\n", getenv("DISPLAY"));
    fprintf(stderr, "DEBUG: ENV SDL_VIDEODRIVER=%s\n", getenv("SDL_VIDEODRIVER"));
    fflush(stderr);

    // USE HARDWARE IF AVAILABLE (Renderer fallback handles the rest)

    memset(&main_dex, 0, sizeof(Pokedex));
    memset(sprites, 0, sizeof(sprites));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); 

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) break;
        }
    }
    IMG_Init(IMG_INIT_PNG);
    if (TTF_Init() < 0) return 1;

    SDL_Window *window = SDL_CreateWindow("Pokedex", 0, 0, screen_w, screen_h, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window) {
        fprintf(stderr, "ERROR: SDL_CreateWindow failed: %s\n", SDL_GetError()); fflush(stderr);
        return 1;
    }
    
    // RENDERER FALLBACK
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "WARN: Accelerated+VSync failed. Trying Accelerated...\n"); fflush(stderr);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!renderer) {
        fprintf(stderr, "WARN: Accelerated failed. Falling back to SOFTWARE renderer.\n"); fflush(stderr);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "ERROR: All renderer attempts failed: %s\n", SDL_GetError()); fflush(stderr);
        return 1;
    }
    SDL_GetRendererOutputSize(renderer, &screen_w, &screen_h);
    
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        fprintf(stderr, "DEBUG: Active Renderer: %s\n", info.name);
        fprintf(stderr, "DEBUG: Renderer Flags: 0x%08X\n", info.flags);
        fprintf(stderr, "DEBUG: Screen Output: %dx%d\n", screen_w, screen_h);
        fflush(stderr);
    }

    card_w = screen_w / GRID_COLS;
    card_h = (screen_h - FOOTER_H) / GRID_ROWS;

    font = TTF_OpenFont("data/font.ttf", FONT_SIZE);
    if (!font) {
        fprintf(stderr, "WARN: data/font.ttf failed. Trying fallback...\n"); fflush(stderr);
        font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", FONT_SIZE);
    }
    if (font) {
        fprintf(stderr, "DEBUG: Font Loaded Successfully.\n"); fflush(stderr);
    } else {
        fprintf(stderr, "ERROR: ALL FONTS FAILED.\n"); fflush(stderr);
    }
    
    char ppath[256];
    sprintf(ppath, "data/icons/pokeball.png");
    ball_tex = IMG_LoadTexture(renderer, ppath);
    if (ball_tex) { fprintf(stderr, "DEBUG: UI Textures Loaded.\n"); fflush(stderr); } else { fprintf(stderr, "ERROR: UI Texture Load Failed: %s\n", IMG_GetError()); fflush(stderr); }

    find_games();

    int selected = 0;
    int scroll = 0;
    bool running = true;
    SDL_Event ev;
    Uint32 last_input = 0;

    while (running) {
        static int frame_count = 0; if (frame_count++ % 120 == 0) { fprintf(stderr, "DEBUG: Heartbeat (frame %d)\n", frame_count); fflush(stderr); }
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            
            if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (app_mode == MODE_GAME_SELECT) {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) { if (selected_game_idx > 0) selected_game_idx--; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) { if (selected_game_idx < game_count - 1) selected_game_idx++; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                        fprintf(stderr, "DEBUG: Selection B (Confirm) index %d\n", selected_game_idx); fflush(stderr);
                        if (load_pokedex(games[selected_game_idx].tsv_path, games[selected_game_idx].caught_path, &main_dex) == 0) {
                            fprintf(stderr, "DEBUG: TSV Loaded. Count: %d\n", main_dex.count);
                            unload_pokedex_assets();
                            fprintf(stderr, "DEBUG: Assets Cleared. Modeswitching...\n"); fflush(stderr);
                            app_mode = MODE_POKEDEX;
                            selected = 0; scroll = 0;
                        } else {
                            fprintf(stderr, "ERROR: TSV Load Failed\n"); fflush(stderr);
                        }
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) { running = false; }
                } else {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                        toggle_caught(&main_dex, selected);
                        save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                        // Refresh count
                        games[selected_game_idx].caught = 0;
                        for(int j=0; j<main_dex.count; j++) if(main_dex.pokemon[j].caught) games[selected_game_idx].caught++;
                        app_mode = MODE_GAME_SELECT;
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) { if (selected >= GRID_COLS) selected -= GRID_COLS; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) { if (selected < main_dex.count - GRID_COLS) selected += GRID_COLS; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) { if (selected > 0) selected--; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) { if (selected < main_dex.count - 1) selected++; }
                }
            }
        }

        const Uint8 *ks = SDL_GetKeyboardState(NULL);
        if (SDL_GetTicks() - last_input > 180) {
            bool moved = false;
            if (app_mode == MODE_GAME_SELECT) {
                if (ks[SDL_SCANCODE_UP]) { if (selected_game_idx > 0) { selected_game_idx--; moved = true; } }
                if (ks[SDL_SCANCODE_DOWN]) { if (selected_game_idx < game_count - 1) { selected_game_idx++; moved = true; } }
                if (ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_Z]) {
                    fprintf(stderr, "DEBUG: Keyboard Enter/Z. Loading game index %d\n", selected_game_idx);
                    if (load_pokedex(games[selected_game_idx].tsv_path, games[selected_game_idx].caught_path, &main_dex) == 0) {
                        fprintf(stderr, "DEBUG: TSV Load Success. Count: %d\n", main_dex.count);
                        unload_pokedex_assets();
                        fprintf(stderr, "DEBUG: Assets Cleared. Modeswitching...\n"); fflush(stderr);
                        app_mode = MODE_POKEDEX;
                        selected = 0; scroll = 0; moved = true;
                    } else {
                        fprintf(stderr, "ERROR: TSV Load Failed\n"); fflush(stderr);
                    }
                }
                if (ks[SDL_SCANCODE_ESCAPE] || ks[SDL_SCANCODE_X]) { running = false; moved = true; }
            } else {
                if (ks[SDL_SCANCODE_UP]) { if (selected >= GRID_COLS) { selected -= GRID_COLS; moved = true; } }
                if (ks[SDL_SCANCODE_DOWN]) { if (selected < main_dex.count - GRID_COLS) { selected += GRID_COLS; moved = true; } }
                if (ks[SDL_SCANCODE_LEFT]) { if (selected > 0) { selected--; moved = true; } }
                if (ks[SDL_SCANCODE_RIGHT]) { if (selected < main_dex.count - 1) { selected++; moved = true; } }
                if (ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_Z]) {
                    toggle_caught(&main_dex, selected);
                    save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                    moved = true;
                }
                if (ks[SDL_SCANCODE_ESCAPE] || ks[SDL_SCANCODE_X] || ks[SDL_SCANCODE_BACKSPACE]) {
                    save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                    games[selected_game_idx].caught = 0;
                    for(int j=0; j<main_dex.count; j++) if(main_dex.pokemon[j].caught) games[selected_game_idx].caught++;
                    app_mode = MODE_GAME_SELECT; moved = true;
                }
            }
            if (moved) last_input = SDL_GetTicks();
        }

        SDL_SetRenderDrawColor(renderer, 65, 55, 45, 255); 
        SDL_RenderClear(renderer);

        if (app_mode == MODE_GAME_SELECT) {
            render_game_select(renderer);
        } else {
            int sel_row = selected / GRID_COLS;
            if (sel_row < scroll) scroll = sel_row;
            if (sel_row >= scroll + GRID_ROWS) scroll = sel_row - GRID_ROWS + 1;

            for (int r = 0; r < GRID_ROWS + 1; r++) {
                int row_idx = scroll + r;
                if (row_idx * GRID_COLS >= main_dex.count) break;
                for (int c = 0; c < GRID_COLS; c++) {
                    int i = row_idx * GRID_COLS + c;
                    if (i >= main_dex.count) break;
                    render_card(renderer, &main_dex, i, c * card_w, r * card_h, i == selected);
                }
            }

            render_footer(renderer);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    unload_pokedex_assets();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
