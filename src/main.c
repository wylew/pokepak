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

SpriteRef sprites[800];
SDL_Texture *ball_tex;
SDL_Texture *ball_x_tex;
SDL_Texture *oak_tex;
SDL_Texture *sil_tex;
TTF_Font *font_list;
TTF_Font *font_bold;
TTF_Font *font_header;

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
        out->tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_QueryTexture(out->tex, NULL, NULL, &out->bbox.w, &out->bbox.h);
        out->bbox.x = 0; out->bbox.y = 0;
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
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font_bold, type, white);
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

    for (int i = 0; i < dex->count; i++) {
        if (i < scroll_off || i >= scroll_off + max_visible) continue;
        int v_idx = i - scroll_off;
        bool sel = (i == selected);
        SDL_Rect item_r = {list_frame.x + 10, list_frame.y + 12 + (int)(v_idx * item_h), list_frame.w - 20, (int)item_h - 2};
        
        if (sel) {
            SDL_Rect highlight = {item_r.x + 2, item_r.y + 2, item_r.w - 4, item_r.h - 4}; // Smaller
            SDL_Color h_red = {255, 0, 0, 255};
            // Use draw_pixel_box with transparent fill for "transparent inside" look
            draw_pixel_box(renderer, highlight, 4, h_red, (SDL_Color){0,0,0,0});
        }

        // Ball Icon
        SDL_Texture *icon = dex->pokemon[i].caught ? ball_tex : ball_x_tex;
        if (icon) {
            SDL_Rect ir = {item_r.x + 10, item_r.y + 10, 40, 40};
            SDL_RenderCopy(renderer, icon, NULL, &ir);
        }

        // ID
        char id_buf[16];
        sprintf(id_buf, "%03d", dex->pokemon[i].dex_id);
        SDL_Color black = {40, 40, 40, 255};
        SDL_Surface *is = TTF_RenderUTF8_Blended(font_list, id_buf, black);
        int id_w = 0;
        if (is) {
            SDL_Texture *it = SDL_CreateTextureFromSurface(renderer, is);
            float s = (float)(item_h - 20) / is->h;
            id_w = (int)(is->w * s);
            SDL_Rect tr = {item_r.x + 60, item_r.y + (item_h - (int)(is->h * s))/2, id_w, (int)(is->h * s)};
            SDL_RenderCopy(renderer, it, NULL, &tr);
            SDL_FreeSurface(is); SDL_DestroyTexture(it);
        }

        // Name
        SDL_Surface *ps = TTF_RenderUTF8_Blended(font_list, dex->pokemon[i].name, black);
        if (ps) {
            SDL_Texture *pt = SDL_CreateTextureFromSurface(renderer, ps);
            float s = (float)(item_h - 20) / ps->h;
            int tw = (int)(ps->w * s);
            int max_tw = item_r.w - 60 - id_w - 30;
            if (tw > max_tw) tw = max_tw;
            int th = (int)(ps->h * s);
            SDL_Rect tr = {item_r.x + 60 + id_w + 15, item_r.y + (item_h - th)/2, tw, th};
            SDL_RenderCopy(renderer, pt, NULL, &tr);
            SDL_FreeSurface(ps); SDL_DestroyTexture(pt);
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
        float s = (float)(name_plate.h - 10) / ns->h; // Larger (+50%)
        int tw = (int)(ns->w * s);
        int max_tw = name_plate.w - 40;
        if (tw > max_tw) tw = max_tw;
        int th = (int)(ns->h * s);
        SDL_Rect tr = {name_plate.x + (name_plate.w - tw)/2, name_plate.y + (name_plate.h - th)/2, tw, th};
        SDL_RenderCopy(renderer, nt, NULL, &tr);
        SDL_FreeSurface(ns); SDL_DestroyTexture(nt);
    }

    // Background Silhouette
    if (sil_tex) {
        int sw, sh;
        SDL_QueryTexture(sil_tex, NULL, NULL, &sw, &sh);
        float aspect = (float)sw / sh;
        int target_h = 420;
        int tw = (int)(target_h * aspect), th = target_h;
        if (tw > 440) { tw = 440; th = (int)(tw / aspect); }
        SDL_Rect sil_r = {50 + (440 - tw)/2, 180 + (420 - th)/2, tw, th};
        SDL_RenderCopy(renderer, sil_tex, NULL, &sil_r);
    }

    // Large Sprite
    if (sprites[selected].tex) {
        SDL_Rect src = sprites[selected].bbox;
        int target_h = 400;
        float aspect = (float)src.w / src.h;
        int tw = (int)(target_h * aspect), th = target_h;
        if (tw > 400) { tw = 400; th = (int)(tw / aspect); }
        SDL_Rect sr = {50 + (440 - tw)/2, 180 + (400 - th)/2, tw, th};
        SDL_RenderCopy(renderer, sprites[selected].tex, NULL, &sr);
    }

    // Type Plate (Baseline aligned at Y=700)
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
    for (int i = 0; i < h; i++) {
        float ratio = (float)i / h;
        Uint8 r = top.r + (bot.r - top.r) * ratio;
        Uint8 g = top.g + (bot.g - top.g) * ratio;
        Uint8 b = top.b + (bot.b - top.b) * ratio;
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawLine(renderer, x, y + i, x + w, y + i);
    }
}

void draw_grid(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    int spacing = 64; // Doubled size
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
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    for(int i=0; i<p; i++) {
        SDL_Rect b = {r.x + i, r.y + i, r.w - (i*2), r.h - (i*2)};
        // Pixel rounding: skip corners
        if (i < p) {
            SDL_RenderDrawLine(renderer, b.x + 6 - i, b.y, b.x + b.w - 7 + i, b.y); // Top
            SDL_RenderDrawLine(renderer, b.x + 6 - i, b.y + b.h - 1, b.x + b.w - 7 + i, b.y + b.h - 1); // Bot
            SDL_RenderDrawLine(renderer, b.x, b.y + 6 - i, b.x, b.y + b.h - 7 + i); // Left
            SDL_RenderDrawLine(renderer, b.x + b.w - 1, b.y + 6 - i, b.x + b.w - 1, b.y + b.h - 7 + i); // Right
            
            // Smoother "pixel" diagonals
            SDL_RenderDrawPoint(renderer, b.x + 1, b.y + 3); SDL_RenderDrawPoint(renderer, b.x + 3, b.y + 1);
            SDL_RenderDrawPoint(renderer, b.x + 2, b.y + 2);
            SDL_RenderDrawPoint(renderer, b.x + b.w - 2, b.y + 3); SDL_RenderDrawPoint(renderer, b.x + b.w - 4, b.y + 1);
            SDL_RenderDrawPoint(renderer, b.x + b.w - 3, b.y + 2);
            SDL_RenderDrawPoint(renderer, b.x + 1, b.y + b.h - 4); SDL_RenderDrawPoint(renderer, b.x + 3, b.y + b.h - 2);
            SDL_RenderDrawPoint(renderer, b.x + 2, b.y + b.h - 3);
            SDL_RenderDrawPoint(renderer, b.x + b.w - 2, b.y + b.h - 4); SDL_RenderDrawPoint(renderer, b.x + b.w - 4, b.y + b.h - 2);
            SDL_RenderDrawPoint(renderer, b.x + b.w - 3, b.y + b.h - 3);
        } else {
            SDL_RenderDrawRect(renderer, &b);
        }
    }
    if (fill.a > 0) {
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_Rect fi = {r.x + p, r.y + p, r.w - (p*2), r.h - (p*2)};
        SDL_RenderFillRect(renderer, &fi);
    }
}

void render_game_select(SDL_Renderer *renderer) {
    // 1. Background Gradient (Light Teal to White)
    SDL_Color teal = {180, 230, 230, 255};
    SDL_Color white = {245, 250, 250, 255};
    draw_gradient(renderer, 0, 0, screen_w, screen_h, teal, white);

    // 2. Draw Prof Oak (Right side)
    if (oak_tex) {
        int ow, oh; SDL_QueryTexture(oak_tex, NULL, NULL, &ow, &oh);
        float scale = (float)(screen_h * 0.65) / oh;
        int tw = (int)(ow * scale), th = (int)(oh * scale);
        SDL_Rect or = {screen_w - tw - 80, screen_h - th - 180, tw, th};
        SDL_RenderCopy(renderer, oak_tex, NULL, &or);
    }

    // 3. Floating List Box (Left side)
    SDL_Rect box_outer = {60, 60, 520, 500};
    SDL_SetRenderDrawColor(renderer, 80, 100, 180, 255); // Blue border
    for(int i=0; i<8; i++) { SDL_Rect b = {box_outer.x+i, box_outer.y+i, box_outer.w-(i*2), box_outer.h-(i*2)}; SDL_RenderDrawRect(renderer, &b); }
    SDL_SetRenderDrawColor(renderer, 248, 248, 248, 255); // White inner
    SDL_Rect box_inner = {box_outer.x+8, box_outer.y+8, box_outer.w-16, box_outer.h-16};
    SDL_RenderFillRect(renderer, &box_inner);

    // Render list items inside the box
    int list_start_y = box_inner.y + 20;
    int max_visible = 7;
    float list_item_h = (float)(box_inner.h - 40) / max_visible;
    int scroll_off = 0;
    if (selected_game_idx >= max_visible) scroll_off = selected_game_idx - max_visible + 1;

    for (int i = 0; i < game_count; i++) {
        if (i < scroll_off || i >= scroll_off + max_visible) continue;
        int v_idx = i - scroll_off;
        bool sel = (i == selected_game_idx);
        SDL_Color color = sel ? (SDL_Color){220, 0, 0, 255} : (SDL_Color){60, 60, 60, 255};
        
        // Selection Highlight (Contracted Parity)
        if (sel) {
            SDL_Rect highlight = {box_inner.x + 10, list_start_y + (int)(v_idx * list_item_h) + 2, box_inner.w - 20, (int)list_item_h - 4};
            SDL_Color h_red = {255, 0, 0, 255};
            draw_pixel_box(renderer, highlight, 4, h_red, (SDL_Color){0,0,0,0});
        }

        SDL_Surface *surf = TTF_RenderUTF8_Blended(sel ? font_bold : font_list, games[i].name, color);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            float s = (float)(list_item_h - 10) / surf->h;
            int tw = (int)(surf->w * s), th = (int)(surf->h * s);
            SDL_Rect tr = {box_inner.x + 30, list_start_y + (int)(v_idx * list_item_h), tw, th};
            SDL_RenderCopy(renderer, tex, NULL, &tr);
            SDL_FreeSurface(surf); SDL_DestroyTexture(tex);
        }
    }

    // 4. Chat Bubble (Bottom)
    SDL_Rect bubble = {40, screen_h - 180, screen_w - 80, 140};
    SDL_Color b_border = {100, 200, 240, 255}, b_white = {255, 255, 255, 255};
    draw_pixel_box(renderer, bubble, 6, b_border, b_white);

    SDL_Color black = {40, 40, 40, 255};
    const char *lines[] = {"Please select your pokemon game", "from the list."};
    for(int i=0; i<2; i++) {
        SDL_Surface *sf = TTF_RenderUTF8_Blended(font_header, lines[i], black);
        if (sf) {
            SDL_Texture *tx = SDL_CreateTextureFromSurface(renderer, sf);
            float s = (float)(bubble.h / 3.0) / sf->h; // Double size (+100%)
            int tw = (int)(sf->w * s), th = (int)(sf->h * s);
            SDL_Rect tr = {bubble.x + (bubble.w - tw)/2, bubble.y + 20 + i * (th + 10), tw, th};
            SDL_RenderCopy(renderer, tx, NULL, &tr);
            SDL_FreeSurface(sf); SDL_DestroyTexture(tx);
        }
    }
}

void render_footer(SDL_Renderer *renderer) {
    SDL_Rect r = {0, screen_h - FOOTER_H, screen_w, FOOTER_H};
    SDL_SetRenderDrawColor(renderer, 50, 40, 30, 255); // Darker wood/brown
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 100, 80, 60, 255); // Accent line
    SDL_RenderDrawLine(renderer, 0, screen_h - FOOTER_H, screen_w, screen_h - FOOTER_H);

    SDL_Color gray = {200, 180, 160, 255}; // Light parchment-gray
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font_header, "[A] Select/Toggle     [B] Back", gray);
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            float scale = (float)(FOOTER_H - 20) / surf->h; // Larger (+100%)
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

#ifdef DESKTOP
    SDL_Window *window = SDL_CreateWindow("Pokedex", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_w, screen_h, SDL_WINDOW_SHOWN);
#else
    SDL_Window *window = SDL_CreateWindow("Pokedex", 0, 0, screen_w, screen_h, SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
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

    font_list = TTF_OpenFont("data/font.ttf", FONT_SIZE_LIST);
    font_bold = TTF_OpenFont("data/font.ttf", FONT_SIZE_BOLD);
    font_header = TTF_OpenFont("data/font.ttf", FONT_SIZE_HEADER);
    if (font_bold) TTF_SetFontStyle(font_bold, TTF_STYLE_BOLD);
    if (font_header) TTF_SetFontStyle(font_header, TTF_STYLE_BOLD);

    if (!font_list) {
        fprintf(stderr, "WARN: data/font.ttf failed. Trying fallback...\n"); fflush(stderr);
        font_list = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", FONT_SIZE_LIST);
    }

    oak_tex = IMG_LoadTexture(renderer, "data/sprites/profoak.png");
    if (!oak_tex) {
        fprintf(stderr, "WARN: profoak.png not found. Trying fallback path...\n"); fflush(stderr);
        oak_tex = IMG_LoadTexture(renderer, "Pokedex.pak/data/sprites/profoak.png");
    }
    if (font_list && font_bold && font_header) {
        fprintf(stderr, "DEBUG: Fonts Loaded Successfully.\n"); fflush(stderr);
    } else {
        fprintf(stderr, "ERROR: FONT LOADING FAILED.\n"); fflush(stderr);
    }
    
    char ppath[256];
    sprintf(ppath, "data/icons/pokeball.png");
    ball_tex = IMG_LoadTexture(renderer, ppath);
    sprintf(ppath, "data/icons/pokeball_x.png");
    ball_x_tex = IMG_LoadTexture(renderer, ppath);
    sil_tex = IMG_LoadTexture(renderer, "data/icons/pokeball_sil.png");
    if (sil_tex) SDL_SetTextureAlphaMod(sil_tex, 51); // 20% alpha
    if (ball_tex && ball_x_tex) { fprintf(stderr, "DEBUG: UI Textures Loaded.\n"); fflush(stderr); } else { fprintf(stderr, "ERROR: UI Texture Load Failed: %s\n", IMG_GetError()); fflush(stderr); }

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
                    else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
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
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) { running = false; }
                } else {
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        toggle_caught(&main_dex, selected);
                        save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                        // Live recount for header
                        games[selected_game_idx].caught = 0;
                        for(int j = 0; j < main_dex.count; j++) if(main_dex.pokemon[j].caught) games[selected_game_idx].caught++;
                    } else if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                        save_caught_status(games[selected_game_idx].caught_path, &main_dex);
                        // Refresh count
                        games[selected_game_idx].caught = 0;
                        for(int j=0; j<main_dex.count; j++) if(main_dex.pokemon[j].caught) games[selected_game_idx].caught++;
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
                    // Live recount for header
                    games[selected_game_idx].caught = 0;
                    for(int j = 0; j < main_dex.count; j++) if(main_dex.pokemon[j].caught) games[selected_game_idx].caught++;
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
            render_pokedex(renderer, &main_dex, selected, scroll);
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
