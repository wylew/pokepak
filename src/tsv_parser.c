#include "pokedex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* trim(char* str) {
    char* end;
    while (*str == ' ' || *str == '\t') str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    end[1] = '\0';
    return str;
}

int load_pokedex(const char *tsv_path, const char *caught_path, Pokedex *dex) {
    FILE *f = fopen(tsv_path, "r");
    if (!f) return -1;

    char line[1024];
    dex->count = 0;

    // Skip header
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    while (fgets(line, sizeof(line), f) && dex->count < MAX_POKEMON) {
        Pokemon *p = &dex->pokemon[dex->count];
        memset(p, 0, sizeof(Pokemon)); // Ensure clear
        char *line_ptr = line;
        int col = 0;

        while (line_ptr && col < 24) {
            char *tab = strchr(line_ptr, '\t');
            char *next_line_ptr = NULL;
            if (tab) {
                *tab = '\0';
                next_line_ptr = tab + 1;
            }
            
            char *val = trim(line_ptr);
            col++;
            switch (col) {
                case 1: p->dex_id = atoi(val); break;
                case 3: strncpy(p->name, val, MAX_STR); break;
                case 4: strncpy(p->type1, val, MAX_STR); break;
                case 5: strncpy(p->type2, val, MAX_STR); break;
                case 6: strncpy(p->form_tag, val, MAX_STR); break;
            }
            line_ptr = next_line_ptr;
        }
        if (p->dex_id > 0) dex->count++;
    }
    fclose(f);

    // Load caught status
    FILE *cf = fopen(caught_path, "r");
    if (cf) {
        int id;
        while (fscanf(cf, "%d", &id) == 1) {
            for (int i = 0; i < dex->count; i++) {
                if (dex->pokemon[i].dex_id == id) {
                    dex->pokemon[i].caught = true;
                }
            }
        }
        fclose(cf);
    }

    return 0;
}

void toggle_caught(Pokedex *dex, int index) {
    if (index >= 0 && index < dex->count) {
        dex->pokemon[index].caught = !dex->pokemon[index].caught;
    }
}

int save_caught_status(const char *caught_path, Pokedex *dex) {
    FILE *f = fopen(caught_path, "w");
    if (!f) return -1;

    for (int i = 0; i < dex->count; i++) {
        if (dex->pokemon[i].caught) {
            fprintf(f, "%d\n", dex->pokemon[i].dex_id);
        }
    }
    fclose(f);
    return 0;
}
