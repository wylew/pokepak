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
#define FONT_SIZE_LIST 70
#define FONT_SIZE_BOLD 105
#define FONT_SIZE_HEADER 140

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

SpriteRef sprites[MAX_POKEMON]; // Match Pokedex size
SDL_Texture *ball_tex;
SDL_Texture *ball_x_tex;
SDL_Texture *oak_tex;
SDL_Texture *sil_tex;
SDL_Texture *bubble_tex[2]; // Cached chat bubble lines
SDL_Texture *footer_tex;    // Cached footer text
TTF_Font *font_list;
TTF_Font *font_bold;
TTF_Font *font_header;

// Parallel Pokedex caches
SDL_Texture *pk_id_tex[MAX_POKEMON];
SDL_Texture *pk_name_tex[MAX_POKEMON];

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
    char custom_sprites_path[512];
    SDL_Texture *name_tex;   // Cached texture for the name
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
void sanitize_pokemon_name(const char *src, char *dest, int mode);
void load_sprite(SDL_Renderer *renderer, Pokemon *p, SpriteRef *out);
void draw_gradient(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color top, SDL_Color bot);
void draw_grid(SDL_Renderer *renderer);
void strip_pokemon(char *str);
void draw_pixel_box(SDL_Renderer *renderer, SDL_Rect r, int p, SDL_Color border, SDL_Color fill);

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
            
            snprintf(games[game_count].custom_sprites_path, 512, "data/games/%s/custom_sprites", dir->d_name);
            
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "name=", 5) == 0) {
                    char *val = line + 5;
                    val[strcspn(val, "\r\n")] = 0;
                    strip_pokemon(val);
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

void sanitize_pokemon_name(const char *src, char *dest, int mode) {
    int j = 0;
    for (int i = 0; src[i]; i++) {
        char c = src[i];
        if (mode == 0) { // Force Lower
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        } else if (mode == 1) { // Force Upper
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        }
        // mode 2: Preserve Original Case (Mixed)

        bool ok = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));

        if (ok) {
            dest[j++] = c;
        } else {
            // Replace any non-alphanumeric (like spaces) with a single underscore
            if (j > 0 && dest[j-1] != '_') dest[j++] = '_';
        }
    }
    if (j > 0 && dest[j-1] == '_') j--;
    dest[j] = 0;
}

SDL_Surface* try_load_from_dir(const char *dir, Pokemon *p) {
    char path[512];
    char sname[128];
    SDL_Surface *surf = NULL;

    // 1. Original Case .png
    sanitize_pokemon_name(p->name, sname, 2);
    sprintf(path, "%s/%s.png", dir, sname);
    surf = IMG_Load(path);
    if (surf) return surf;

    // 2. lowercase .png
    sanitize_pokemon_name(p->name, sname, 0);
    sprintf(path, "%s/%s.png", dir, sname);
    surf = IMG_Load(path);
    if (surf) return surf;

    // 3. UPPERCASE .PNG
    sanitize_pokemon_name(p->name, sname, 1);
    sprintf(path, "%s/%s.PNG", dir, sname);
    surf = IMG_Load(path);
    if (surf) return surf;

    // 4. UPPERCASE .png
    sprintf(path, "%s/%s.png", dir, sname);
    surf = IMG_Load(path);
    if (surf) return surf;

    // 5. lowercase .PNG
    sanitize_pokemon_name(p->name, sname, 0);
    sprintf(path, "%s/%s.PNG", dir, sname);
    surf = IMG_Load(path);
    if (surf) return surf;

    // 6. National ID
    sprintf(path, "%s/%d.png", dir, p->national_id);
    surf = IMG_Load(path);
    return surf;
}

void load_sprite(SDL_Renderer *renderer, Pokemon *p, SpriteRef *out) {
    SDL_Surface *surf = NULL;

    // 1. Try Game-Specific Custom Sprites First
    surf = try_load_from_dir(games[selected_game_idx].custom_sprites_path, p);
    if (surf) fprintf(stderr, "DEBUG: Found Custom Game Sprite for %s\n", p->name);

    // 2. Try Global Sprites
    if (!surf) {
        surf = try_load_from_dir("data/sprites", p);
    }



    if (surf) {
        out->tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_QueryTexture(out->tex, NULL, NULL, &out->bbox.w, &out->bbox.h);
        out->bbox.x = 0; out->bbox.y = 0;
        SDL_FreeSurface(surf);
    }
}

void unload_pokedex_assets() {
    for (int i = 0; i < MAX_POKEMON; i++) {
        if (sprites[i].tex) {
            SDL_DestroyTexture(sprites[i].tex);
            sprites[i].tex = NULL;
        }
    }
    // Clean up cached game textures
    for (int i = 0; i < 64; i++) {
        if (games[i].name_tex) {
            SDL_DestroyTexture(games[i].name_tex);
            games[i].name_tex = NULL;
        }
    }
    // Clean up bubble and footer textures
    for (int i = 0; i < 2; i++) {
        if (bubble_tex[i]) {
            SDL_DestroyTexture(bubble_tex[i]);
            bubble_tex[i] = NULL;
        }
    }
    if (footer_tex) {
        SDL_DestroyTexture(footer_tex);
        footer_tex = NULL;
    }
    // Clean up Pokedex caches
    for (int i = 0; i < MAX_POKEMON; i++) {
        if (pk_id_tex[i]) { SDL_DestroyTexture(pk_id_tex[i]); pk_id_tex[i] = NULL; }
        if (pk_name_tex[i]) { SDL_DestroyTexture(pk_name_tex[i]); pk_name_tex[i] = NULL; }
    }
}

// Pre-render game names to avoid texture creation in the main loop
void cache_game_textures(SDL_Renderer *renderer) {
    SDL_Color black = {40, 40, 40, 255};
    for (int i = 0; i < game_count; i++) {
        if (games[i].name_tex) SDL_DestroyTexture(games[i].name_tex);
        SDL_Surface *s = TTF_RenderUTF8_Blended(font_list, games[i].name, black);
        if (s) {
            games[i].name_tex = SDL_CreateTextureFromSurface(renderer, s);
            SDL_FreeSurface(s);
        }
    }

    // Cache Bubble Text
    const char *lines[] = {"Please select your pokemon game", "from the list."};
    for(int i=0; i<2; i++) {
        if (bubble_tex[i]) SDL_DestroyTexture(bubble_tex[i]);
        SDL_Surface *sf = TTF_RenderUTF8_Blended(font_header, lines[i], black);
        if (sf) {
            bubble_tex[i] = SDL_CreateTextureFromSurface(renderer, sf);
            SDL_FreeSurface(sf);
        }
    }

    // Cache Footer Text
    SDL_Color gray = {200, 180, 160, 255};
    if (footer_tex) SDL_DestroyTexture(footer_tex);
    SDL_Surface *fs = TTF_RenderUTF8_Blended(font_header, "[A] Select/Toggle     [B] Back", gray);
    if (fs) {
        footer_tex = SDL_CreateTextureFromSurface(renderer, fs);
        SDL_FreeSurface(fs);
    }

    // Cache Pokedex names/IDs for the CURRENT main_dex
    SDL_Color gray_c = {120, 120, 120, 255};
    SDL_Color black_c = {40, 40, 40, 255};
    for (int i = 0; i < main_dex.count; i++) {
        if (pk_id_tex[i]) SDL_DestroyTexture(pk_id_tex[i]);
        if (pk_name_tex[i]) SDL_DestroyTexture(pk_name_tex[i]);
        
        char id_str[16]; sprintf(id_str, "#%03d", main_dex.pokemon[i].dex_id);
        SDL_Surface *s1 = TTF_RenderUTF8_Blended(font_list, id_str, gray_c);
        if (s1) { pk_id_tex[i] = SDL_CreateTextureFromSurface(renderer, s1); SDL_FreeSurface(s1); }
        
        SDL_Surface *s2 = TTF_RenderUTF8_Blended(font_list, main_dex.pokemon[i].name, black_c);
        if (s2) { pk_name_tex[i] = SDL_CreateTextureFromSurface(renderer, s2); SDL_FreeSurface(s2); }
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
    // Uppercase the type name for display
    char upper_type[64];
    int ui = 0;
    for (; type[ui] && ui < 63; ui++) upper_type[ui] = (type[ui] >= 'a' && type[ui] <= 'z') ? type[ui] - 32 : type[ui];
    upper_type[ui] = '\0';
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font_bold, upper_type, white);

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

void render_pokedex(SDL_Renderer *renderer, Pokedex *dex, int selected, int scroll) {
    if (!dex || dex->count == 0) return;
    // 1. Background Grid
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    draw_grid(renderer);

    // 2. Top Red Bar
    SDL_Rect top_bar = {0, 0, screen_w, 80};
    SDL_Color red_top = {220, 20, 20, 255}, red_bot = {160, 0, 0, 255};
    draw_gradient(renderer, top_bar.x, top_bar.y, top_bar.w, top_bar.h, red_top, red_bot);
    
    char title[128];
    sprintf(title, "%s (%d / %d)", games[selected_game_idx].name, games[selected_game_idx].caught, dex->count);
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *ts = TTF_RenderUTF8_Blended(font_header, title, white);
    if (ts) {
        SDL_Texture *tt = SDL_CreateTextureFromSurface(renderer, ts);
        float s = (float)(top_bar.h - 10) / ts->h; // Larger
        int tw = (int)(ts->w * s), th = (int)(ts->h * s);
        SDL_Rect tr = {(screen_w - tw)/2, (top_bar.h - th)/2, tw, th};
        SDL_RenderCopy(renderer, tt, NULL, &tr);
        SDL_FreeSurface(ts); SDL_DestroyTexture(tt);
    }

    // 3. Right side: Vertical List
    SDL_Rect list_frame = {520, 100, 460, 600}; // Ends at Y=700
    SDL_Color r_border = {200, 40, 40, 255}, r_white = {255, 255, 255, 220};
    draw_pixel_box(renderer, list_frame, 8, r_border, r_white);

    int max_visible = 7;
    float item_h = (float)(list_frame.h - 16 - 20) / max_visible; // Subtracting 8px border*2 and 10px padding*2
    int scroll_off = (selected >= max_visible) ? selected - max_visible + 1 : 0;

    int id_w = 0;
    for (int i = 0; i < dex->count; i++) {
        if (i < scroll_off || i >= scroll_off + max_visible) continue;
        int v_idx = i - scroll_off;
        bool sel = (i == selected);
        SDL_Rect item_r = {list_frame.x + 10, list_frame.y + 12 + (int)(v_idx * item_h), list_frame.w - 20, (int)item_h - 2};
        
        if (sel) {
            SDL_Rect highlight = {item_r.x + 2, item_r.y + 2, item_r.w - 4, item_r.h - 4}; // Smaller
            SDL_Color h_red = {255, 0, 0, 255};
            draw_pixel_box(renderer, highlight, 4, h_red, (SDL_Color){0,0,0,0});
        }



        // Ball Icon — vertically centered in row
        SDL_Texture *icon = dex->pokemon[i].caught ? ball_tex : ball_x_tex;
        if (icon) {
            int icon_size = (int)(item_h - 20);
            if (icon_size > 48) icon_size = 48; // Cap at 48px
            SDL_Rect ir = {item_r.x + 10, item_r.y + ((int)item_h - icon_size) / 2, icon_size, icon_size};
            SDL_RenderCopy(renderer, icon, NULL, &ir);
        }

        // ID (Cached)
        if (pk_id_tex[i]) {
            int tw, th; SDL_QueryTexture(pk_id_tex[i], NULL, NULL, &tw, &th);
            float s = (float)(item_h - 20) / th;
            id_w = (int)(tw * s);
            SDL_Rect tr = {item_r.x + 60, item_r.y + (item_h - (int)(th * s))/2, id_w, (int)(th * s)};
            SDL_RenderCopy(renderer, pk_id_tex[i], NULL, &tr);
        }

        // Name (Cached)
        if (pk_name_tex[i]) {
            int tw, th; SDL_QueryTexture(pk_name_tex[i], NULL, NULL, &tw, &th);
            float s = (float)(item_h - 20) / th;
            int tw_scaled = (int)(tw * s);
            int max_tw = list_frame.w - 100 - id_w;
            if (tw_scaled > max_tw) tw_scaled = max_tw;
            SDL_Rect tr = {item_r.x + 60 + id_w + 15, item_r.y + (item_h - (int)(th * s))/2, tw_scaled, (int)(th * s)};
            SDL_RenderCopy(renderer, pk_name_tex[i], NULL, &tr);
        }
    }

    // 4. Left side: Detail Focus
    Pokemon *p = &dex->pokemon[selected];
    if (!sprites[selected].tex) load_sprite(renderer, p, &sprites[selected]);

    // Name Plate (Squeezed for long names)
    SDL_Rect name_plate = {50, 100, 440, 70};
    draw_pixel_box(renderer, name_plate, 8, r_border, r_white);


    SDL_Surface *ns = TTF_RenderUTF8_Blended(font_bold, p->name, (SDL_Color){40, 40, 40, 255});
    if (ns) {
        SDL_Texture *nt = SDL_CreateTextureFromSurface(renderer, ns);
        float s = (float)(name_plate.h - 10) / ns->h;
        int tw = (int)(ns->w * s);
        int max_tw = name_plate.w - 40;
        if (tw > max_tw) tw = max_tw;
        int th = (int)(ns->h * s);
        SDL_Rect tr = {name_plate.x + (name_plate.w - tw)/2, name_plate.y + (name_plate.h - th)/2, tw, th};
        SDL_RenderCopy(renderer, nt, NULL, &tr);
        SDL_FreeSurface(ns); SDL_DestroyTexture(nt);
    }

    // Silhouette
    if (sil_tex) {
        int sw, sh; SDL_QueryTexture(sil_tex, NULL, NULL, &sw, &sh);
        float aspect = (float)sw / sh;
        int th = 420, tw = (int)(420 * aspect);
        if (tw > 440) { tw = 440; th = (int)(440 / aspect); }
        SDL_Rect sil_r = {50 + (440 - tw)/2, 180 + (420 - th)/2, tw, th};
        SDL_RenderCopy(renderer, sil_tex, NULL, &sil_r);
    }

    // Large Sprite
    if (sprites[selected].tex) {
        SDL_Rect src = sprites[selected].bbox;
        int th = 400, tw = (int)(400 * ((float)src.w / src.h));
        if (tw > 400) { tw = 400; th = (int)(400 / ((float)src.w / src.h)); }
        SDL_Rect sr = {50 + (440 - tw)/2, 180 + (400 - th)/2, tw, th};
        SDL_RenderCopy(renderer, sprites[selected].tex, NULL, &sr);
    }



    // Type Plate
    SDL_Rect type_plate = {50, 610, 440, 90};
    draw_pixel_box(renderer, type_plate, 8, r_border, r_white);
    int badge_w = 180, badge_h = 50;


    if (p->type1[0] && !p->type2[0]) {
        draw_type_tag(renderer, p->type1, type_plate.x + (type_plate.w - badge_w)/2, type_plate.y + (type_plate.h - badge_h)/2, badge_w, badge_h);
    } else {
        if (p->type1[0]) draw_type_tag(renderer, p->type1, type_plate.x + 20, type_plate.y + (type_plate.h - badge_h)/2, badge_w, badge_h);
        if (p->type2[0]) draw_type_tag(renderer, p->type2, type_plate.x + type_plate.w - badge_w - 20, type_plate.y + (type_plate.h - badge_h)/2, badge_w, badge_h);
    }

    // 5. Global Bottom Bar
    SDL_Rect foot_bar = {0, screen_h - 60, screen_w, 60};
    draw_gradient(renderer, foot_bar.x, foot_bar.y, foot_bar.w, foot_bar.h, red_top, red_bot);
    SDL_Surface *is = TTF_RenderUTF8_Blended(font_header, "[A] Toggle Caught    [B] Back", white);
    if (is) {
        SDL_Texture *it = SDL_CreateTextureFromSurface(renderer, is);
        float s = (float)(foot_bar.h - 10) / is->h;
        int tw = (int)(is->w * s), th = (int)(is->h * s);
        SDL_Rect tr = {(screen_w - tw)/2, foot_bar.y + (foot_bar.h - th)/2, tw, th};
        SDL_RenderCopy(renderer, it, NULL, &tr);
        SDL_FreeSurface(is); SDL_DestroyTexture(it);
    }
}

void draw_gradient(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color top, SDL_Color bot) {
    int step = 4; 
    for (int i = 0; i < h; i += step) {
        float ratio = (float)i / h;
        Uint8 r = top.r + (bot.r - top.r) * ratio;
        Uint8 g = top.g + (bot.g - top.g) * ratio;
        Uint8 b = top.b + (bot.b - top.b) * ratio;
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_Rect rect = {x, y + i, w, (i + step > h) ? (h - i) : step};
        SDL_RenderFillRect(renderer, &rect);
    }
}

void draw_grid(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    int spacing = 64; 
    for (int x = 0; x < screen_w; x += spacing) SDL_RenderDrawLine(renderer, x, 0, x, screen_h);
    for (int y = 0; y < screen_h; y += spacing) SDL_RenderDrawLine(renderer, 0, y, screen_w, y);
}



void strip_pokemon(char *str) {
    const char *targets[] = {"Pokemon ", "pokemon ", "POKEMON ", "Pokemon", "pokemon", "POKEMON"};
    for (int i = 0; i < 6; i++) {
        char *p = strstr(str, targets[i]);
        if (p) {
            int len = strlen(targets[i]);
            memmove(p, p + len, strlen(p + len) + 1);
            break;
        }
    }
}

void draw_pixel_box(SDL_Renderer *renderer, SDL_Rect r, int p, SDL_Color border, SDL_Color fill) {
    if (fill.a > 0) {
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &r);
    }
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    for(int i=0; i<p; i++) {
        SDL_Rect b = {r.x + i, r.y + i, r.w - (i*2), r.h - (i*2)};
        SDL_RenderDrawLine(renderer, b.x + 2, b.y, b.x + b.w - 3, b.y);             // Top
        SDL_RenderDrawLine(renderer, b.x + 2, b.y + b.h - 1, b.x + b.w - 3, b.y + b.h - 1); // Bot
        SDL_RenderDrawLine(renderer, b.x, b.y + 2, b.x, b.y + b.h - 3);             // Left
        SDL_RenderDrawLine(renderer, b.x + b.w - 1, b.y + 2, b.x + b.w - 1, b.y + b.h - 3); // Right
        SDL_RenderDrawPoint(renderer, b.x + 1, b.y + 1);
        SDL_RenderDrawPoint(renderer, b.x + b.w - 2, b.y + 1);
        SDL_RenderDrawPoint(renderer, b.x + 1, b.y + b.h - 2);
        SDL_RenderDrawPoint(renderer, b.x + b.w - 2, b.y + b.h - 2);
    }
}

void render_game_select(SDL_Renderer *renderer) {
    // ── GBA-style banded gradient ─────────────────────────────────────────────
    // Colors from the reference: teal top, light mint/white center,
    // slightly dimmer teal bottom. Non-linear: holds top 30%, bottom 30%.
    // Each band is 8px tall to create the visible banding artifact.
    {
        // Top solid zone (30% of screen height)
        int top_zone    = (int)(screen_h * 0.30f);
        // Bottom solid zone (30% of screen height)
        int bot_zone    = (int)(screen_h * 0.30f);
        // Transition zone (middle 40%)
        int mid_start   = top_zone;
        int mid_end     = screen_h - bot_zone;
        int mid_h       = mid_end - mid_start;
        int band        = 8; // pixel band height

        // Top section – solid teal  #A8D8D0  (168,216,208)
        SDL_SetRenderDrawColor(renderer, 168, 216, 208, 255);
        SDL_Rect top_r = {0, 0, screen_w, top_zone};
        SDL_RenderFillRect(renderer, &top_r);

        // Middle section – banded transition teal→white
        for (int y = mid_start; y < mid_end; y += band) {
            float t = (float)(y - mid_start) / mid_h;
            // Ease in-out curve to make the bands look intentional
            t = t * t * (3.0f - 2.0f * t);
            Uint8 r = (Uint8)(168 + (230 - 168) * t);
            Uint8 g = (Uint8)(216 + (245 - 216) * t);
            Uint8 b = (Uint8)(208 + (242 - 208) * t);
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            int bh = (y + band > mid_end) ? (mid_end - y) : band;
            SDL_Rect br = {0, y, screen_w, bh};
            SDL_RenderFillRect(renderer, &br);
        }

        // Bottom section – solid slightly darker teal  #88C8C0  (136,200,192)
        SDL_SetRenderDrawColor(renderer, 136, 200, 192, 255);
        SDL_Rect bot_r = {0, mid_end, screen_w, screen_h - mid_end};
        SDL_RenderFillRect(renderer, &bot_r);
    }

    if (oak_tex) {
        int ow, oh; SDL_QueryTexture(oak_tex, NULL, NULL, &ow, &oh);
        float scale = (float)(screen_h * 0.65) / oh;
        int tw = (int)(ow * scale), th = (int)(oh * scale);
        SDL_Rect or = {screen_w - tw - 80, screen_h - th - 180, tw, th};
        SDL_RenderCopy(renderer, oak_tex, NULL, &or);
    }
    SDL_Rect box_outer = {60, 60, 520, 500};
    SDL_SetRenderDrawColor(renderer, 80, 100, 180, 255); 
    for(int i=0; i<8; i++) { SDL_Rect b = {box_outer.x+i, box_outer.y+i, box_outer.w-(i*2), box_outer.h-(i*2)}; SDL_RenderDrawRect(renderer, &b); }
    SDL_SetRenderDrawColor(renderer, 248, 248, 248, 255); 
    SDL_Rect box_inner = {box_outer.x+8, box_outer.y+8, box_outer.w-16, box_outer.h-16};
    SDL_RenderFillRect(renderer, &box_inner);

    int list_start_y = box_inner.y + 20, max_visible = 7;
    float list_item_h = (float)(box_inner.h - 40) / max_visible;
    int scroll_off = (selected_game_idx >= max_visible) ? selected_game_idx - max_visible + 1 : 0;

    for (int i = 0; i < game_count; i++) {
        int v_idx = i - scroll_off;
        if (v_idx < 0 || v_idx >= max_visible) continue;
        if (i == selected_game_idx) {
            SDL_Rect highlight = {box_inner.x + 10, list_start_y + (int)(v_idx * list_item_h) + 2, box_inner.w - 20, (int)list_item_h - 4};
            draw_pixel_box(renderer, highlight, 4, (SDL_Color){255, 0, 0, 255}, (SDL_Color){0,0,0,0});
        }
        if (games[i].name_tex) {
            int tw, th; SDL_QueryTexture(games[i].name_tex, NULL, NULL, &tw, &th);
            float s = (float)(list_item_h - 10) / th;
            SDL_Rect tr = {box_inner.x + 30, list_start_y + (int)(v_idx * list_item_h), (int)(tw*s), (int)(th*s)};
            SDL_RenderCopy(renderer, games[i].name_tex, NULL, &tr);
        }
    }
    // ── Chat bubble (GBA-style: white fill, thin rounded light-blue border) ──
    SDL_Rect bubble = {30, screen_h - 185, screen_w - 60, 148};
    // Fill
    SDL_SetRenderDrawColor(renderer, 252, 252, 252, 255);
    SDL_RenderFillRect(renderer, &bubble);
    // Thin border – draw a 2-pixel inset rounded rect manually
    SDL_Color bdr = {150, 210, 220, 255};
    SDL_SetRenderDrawColor(renderer, bdr.r, bdr.g, bdr.b, 255);
    // Outer frame (2px thick)
    for (int t = 0; t < 2; t++) {
        SDL_Rect f = {bubble.x + t, bubble.y + t, bubble.w - t*2, bubble.h - t*2};
        // Top & bottom lines (with 8px corner gap)
        SDL_RenderDrawLine(renderer, f.x + 8, f.y,         f.x + f.w - 8, f.y);
        SDL_RenderDrawLine(renderer, f.x + 8, f.y+f.h-1,  f.x + f.w - 8, f.y+f.h-1);
        // Left & right lines (with 8px gap)
        SDL_RenderDrawLine(renderer, f.x,         f.y + 8, f.x,         f.y + f.h - 8);
        SDL_RenderDrawLine(renderer, f.x+f.w-1,  f.y + 8, f.x+f.w-1,   f.y + f.h - 8);
        // Corners – 45-degree diagonal pixel
        SDL_RenderDrawPoint(renderer, f.x + 4, f.y + 2);
        SDL_RenderDrawPoint(renderer, f.x + 3, f.y + 3);
        SDL_RenderDrawPoint(renderer, f.x + 2, f.y + 4);
        SDL_RenderDrawPoint(renderer, f.x + f.w - 5, f.y + 2);
        SDL_RenderDrawPoint(renderer, f.x + f.w - 4, f.y + 3);
        SDL_RenderDrawPoint(renderer, f.x + f.w - 3, f.y + 4);
        SDL_RenderDrawPoint(renderer, f.x + 4, f.y + f.h - 3);
        SDL_RenderDrawPoint(renderer, f.x + 3, f.y + f.h - 4);
        SDL_RenderDrawPoint(renderer, f.x + 2, f.y + f.h - 5);
        SDL_RenderDrawPoint(renderer, f.x + f.w - 5, f.y + f.h - 3);
        SDL_RenderDrawPoint(renderer, f.x + f.w - 4, f.y + f.h - 4);
        SDL_RenderDrawPoint(renderer, f.x + f.w - 3, f.y + f.h - 5);
    }
    // Text lines left-aligned (like GBA dialogue box)
    // Scaled to match the 60px height of the name plate in pokedex view
    int text_y = bubble.y + 14; 
    for (int i = 0; i < 2; i++) {
        if (bubble_tex[i]) {
            int tw, th; SDL_QueryTexture(bubble_tex[i], NULL, NULL, &tw, &th);
            float target_h = 60.0f; 
            float s = target_h / th;
            SDL_Rect tr = {bubble.x + 28, text_y, (int)(tw*s), (int)(th*s)};
            SDL_RenderCopy(renderer, bubble_tex[i], NULL, &tr);
            text_y += (int)(th*s) + 4; // Tight spacing to fit two 60px lines
        }
    }
}



void render_footer(SDL_Renderer *renderer) {
    SDL_Rect r = {0, screen_h - FOOTER_H, screen_w, FOOTER_H};
    SDL_SetRenderDrawColor(renderer, 50, 40, 30, 255); 
    SDL_RenderFillRect(renderer, &r);
    if (footer_tex) {
        int tw, th; SDL_QueryTexture(footer_tex, NULL, NULL, &tw, &th);
        float scale = (float)(FOOTER_H - 18) / th;
        int tw_s = (int)(tw * scale);
        if (tw_s > screen_w - 40) { scale = (float)(screen_w - 40) / tw; tw_s = screen_w - 40; }
        SDL_Rect tr = {(screen_w - tw_s)/2, screen_h - FOOTER_H + (FOOTER_H - (int)(th*scale))/2, tw_s, (int)(th*scale)};
        SDL_RenderCopy(renderer, footer_tex, NULL, &tr);
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "DEBUG: App Started. Pokedex Init.\n");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    SDL_SetHint(SDL_HINT_RENDER_OPENGL_SHADERS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) return 1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); 

    memset(&main_dex, 0, sizeof(Pokedex));
    memset(sprites, 0, sizeof(sprites));
    memset(pk_id_tex, 0, sizeof(pk_id_tex));
    memset(pk_name_tex, 0, sizeof(pk_name_tex));

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) break;
        }
    }
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

#ifdef DESKTOP
    SDL_Window *window = SDL_CreateWindow("Pokedex", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_w, screen_h, SDL_WINDOW_SHOWN);
#else
    SDL_Window *window = SDL_CreateWindow("Pokedex", 0, 0, screen_w, screen_h, 0); // Safer than FULLSCREEN
#endif
    if (!window) { fprintf(stderr, "ERROR: Window Creation Failed: %s\n", SDL_GetError()); return 1; }
    SDL_Delay(100); // Driver settle

    // LOG AVAILABLE DRIVERS
    int num_drivers = SDL_GetNumRenderDrivers();
    fprintf(stderr, "DEBUG: Available Render Drivers (%d):\n", num_drivers);
    for (int i = 0; i < num_drivers; i++) {
        SDL_RendererInfo ri; SDL_GetRenderDriverInfo(i, &ri);
        fprintf(stderr, "  %d: %s (flags: 0x%08X)\n", i, ri.name, ri.flags);
    }

    // FORCE GLES Hints
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_LoadLibrary(NULL); 

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "WARN: GLES2+VSync failed, trying ANY accelerated.\n");
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!renderer) { 
        fprintf(stderr, "WARN: Accelerated failed, Falling back to SOFTWARE.\n");
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) return 1;

    SDL_GetRendererOutputSize(renderer, &screen_w, &screen_h);
    card_w = screen_w / GRID_COLS; card_h = (screen_h - FOOTER_H) / GRID_ROWS;

    font_list = TTF_OpenFont("data/font.ttf", FONT_SIZE_LIST);
    font_bold = TTF_OpenFont("data/font.ttf", FONT_SIZE_BOLD);
    font_header = TTF_OpenFont("data/font.ttf", FONT_SIZE_HEADER);
    if (font_bold) TTF_SetFontStyle(font_bold, TTF_STYLE_BOLD);
    if (font_header) TTF_SetFontStyle(font_header, TTF_STYLE_BOLD);

    oak_tex = IMG_LoadTexture(renderer, "data/sprites/profoak.png");
    ball_tex = IMG_LoadTexture(renderer, "data/icons/pokeball.png");
    ball_x_tex = IMG_LoadTexture(renderer, "data/icons/pokeball_x.png");
    sil_tex = IMG_LoadTexture(renderer, "data/icons/pokeball_sil.png");
    if (sil_tex) SDL_SetTextureAlphaMod(sil_tex, 51);

    find_games(); cache_game_textures(renderer);

    int selected = 0, scroll = 0;
    bool running = true;
    SDL_Event ev;
    Uint32 last_input = 0;

    fprintf(stderr, "DEBUG: Starting Main Loop\n"); fflush(stderr);
    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { fprintf(stderr, "DEBUG: SDL_QUIT received\n"); running = false; }
            if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                if (app_mode == MODE_GAME_SELECT) {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) { if (selected_game_idx > 0) selected_game_idx--; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) { if (selected_game_idx < game_count - 1) selected_game_idx++; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) { // B = confirm on TRIMUI (Nintendo layout)
                        fprintf(stderr, "DEBUG: B pressed. Loading index %d\n", selected_game_idx);
                        if (load_pokedex(games[selected_game_idx].tsv_path, games[selected_game_idx].caught_path, &main_dex) == 0) {
                            unload_pokedex_assets(); cache_game_textures(renderer);
                            app_mode = MODE_POKEDEX; selected = 0; scroll = 0;
                        }
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) { fprintf(stderr, "DEBUG: A pressed (Quit)\n"); running = false; } // A = back/quit on TRIMUI
                } else {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) { // B = confirm/toggle on TRIMUI
                        toggle_caught(&main_dex, selected);
                        save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) { // A = back on TRIMUI
                        save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                        app_mode = MODE_GAME_SELECT;
                    }
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) { if (selected > 0) selected--; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) { if (selected < main_dex.count - 1) selected++; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) { selected -= 10; if (selected < 0) selected = 0; }
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) { selected += 10; if (selected >= main_dex.count) selected = main_dex.count - 1; }
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
                    if (load_pokedex(games[selected_game_idx].tsv_path, games[selected_game_idx].caught_path, &main_dex) == 0) {
                        unload_pokedex_assets(); cache_game_textures(renderer);
                        app_mode = MODE_POKEDEX; selected = 0; scroll = 0; moved = true;
                    }
                }
            } else {
                if (ks[SDL_SCANCODE_UP]) { if (selected >= GRID_COLS) { selected -= GRID_COLS; moved = true; } }
                if (ks[SDL_SCANCODE_DOWN]) { if (selected < main_dex.count - GRID_COLS) { selected += GRID_COLS; moved = true; } }
                if (ks[SDL_SCANCODE_LEFT]) { if (selected > 0) { selected--; moved = true; } }
                if (ks[SDL_SCANCODE_RIGHT]) { if (selected < main_dex.count - 1) { selected++; moved = true; } }
                if (ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_Z]) { toggle_caught(&main_dex, selected); moved = true; }
                if (ks[SDL_SCANCODE_ESCAPE] || ks[SDL_SCANCODE_X]) { app_mode = MODE_GAME_SELECT; moved = true; }
            }
            if (moved) last_input = SDL_GetTicks();
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        if (app_mode == MODE_GAME_SELECT) render_game_select(renderer);
        else render_pokedex(renderer, &main_dex, selected, scroll);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    unload_pokedex_assets();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}
