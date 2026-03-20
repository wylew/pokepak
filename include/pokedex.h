#ifndef POKEDEX_H
#define POKEDEX_H

#include <stdbool.h>

#define MAX_POKEMON 2048
#define MAX_STR 128

typedef struct {
    int dex_id;
    int national_id;
    char name[MAX_STR];
    char type1[MAX_STR];
    char type2[MAX_STR];
    char form_tag[MAX_STR];
    bool caught;
} Pokemon;

typedef struct {
    Pokemon pokemon[MAX_POKEMON];
    int count;
    char game_slug[MAX_STR];
} Pokedex;

// Function prototypes
int load_pokedex(const char *tsv_path, const char *caught_path, Pokedex *dex);
int save_caught_status(const char *caught_path, Pokedex *dex);
void toggle_caught(Pokedex *dex, int index);

#endif
