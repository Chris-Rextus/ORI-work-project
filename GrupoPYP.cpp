/*
Grupo PYP
Integrantes:
Pedro Augusto Faria - 821124
Pedro Yudi Teixeira Harada - 800636
Yuri Bastos Wirthmann - 812311
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_HASH_SIZE   211
#define TITLE_HASH_SIZE  211
#define MAX_LINE         512
#define MAX_NAME         128
#define MAX_TITLE        256

/* ---------- structs ---------- */

typedef struct IntList {
    int value;
    struct IntList* next;
} IntList;

typedef struct NameEntry {
    char name[MAX_NAME];
    int  id;
    struct NameEntry* next;
} NameEntry;

typedef struct TitleEntry {
    char title[MAX_TITLE];
    IntList* authorIds;
    struct TitleEntry* next;
} TitleEntry;

typedef struct Edge {
    int neighborId;
    /* lista de títulos em comum, ex: array/linked list de strings */
    struct Edge* next;
} Edge;

typedef struct GraphNode {
    int id;
    Edge* edges;
} GraphNode;

/* ---------- tables (globals or passed by pointer, your call) ---------- */

NameEntry*  g_nameHash[NAME_HASH_SIZE];
TitleEntry* g_titleHash[TITLE_HASH_SIZE];

char*       g_index[1000];   /* IdP -> name, resize as needed */
GraphNode   g_graph[1000];   /* IdP -> node */
int         g_nodeCount = 0;

/* ---------- hashing ---------- */

unsigned int hashString(const char* s, unsigned int tableSize) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++); /* djb2 */
    return h % tableSize;
}

/* ---------- name table ---------- */

int getOrCreateResearcherId(const char* name) {
    /* TODO: lookup in g_nameHash; if found return id;
       else insert new NameEntry, assign next id, update g_index and g_graph */
    return -1;
}

/* ---------- title table ---------- */

void registerTitleAuthor(const char* title, int authorId) {
    /* TODO: lookup title in g_titleHash;
       - if not found: create TitleEntry
       - if found (traditional collision: different title) -> chain
       - if found (repeated collision: same title) -> append authorId to authorIds */
}

/* ---------- graph construction ---------- */

void connectCollaborators(void) {
    /* TODO: for each TitleEntry with 2+ authorIds,
       connect every pair with an edge, storing the title */
}

/* ---------- parsing ---------- */

void loadFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { perror("fopen"); exit(1); }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char* tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';
        char* name  = line;
        char* title = tab + 1;
        title[strcspn(title, "\r\n")] = '\0';

        int id = getOrCreateResearcherId(name);
        registerTitleAuthor(title, id);
    }
    fclose(f);
    connectCollaborators();
}

/* ---------- operations ---------- */

void listCollaborators(const char* name) {
    /* TODO */
}

void listAuthors(const char* title) {
    /* TODO */
}

int maxDegree(void) {
    /* TODO */
    return 0;
}

double avgDegree(void) {
    /* TODO */
    return 0.0;
}

/* ---------- main ---------- */

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s <arquivo>\n", argv[0]);
        return 1;
    }

    loadFile(argv[1]);

    int option;
    char buf[MAX_NAME];
    do {
        printf("\n1) Colaboradores de um nome\n"
               "2) Autores de um titulo\n"
               "3) Maior grau\n"
               "4) Grau medio\n"
               "0) Sair\n> ");
        if (scanf("%d", &option) != 1) break;
        getchar();

        switch (option) {
            case 1:
                printf("Nome: ");
                fgets(buf, sizeof(buf), stdin);
                buf[strcspn(buf, "\r\n")] = '\0';
                listCollaborators(buf);
                break;
            case 2:
                printf("Titulo: ");
                fgets(buf, sizeof(buf), stdin);
                buf[strcspn(buf, "\r\n")] = '\0';
                listAuthors(buf);
                break;
            case 3:
                printf("Maior grau: %d\n", maxDegree());
                break;
            case 4:
                printf("Grau medio: %.2f\n", avgDegree());
                break;
        }
    } while (option != 0);

    return 0;
}