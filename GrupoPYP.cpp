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

#define NAME_HASH_SIZE   100003
#define TITLE_HASH_SIZE  100003
#define MAX_LINE         4096
#define MAX_QUERY        1024

/* ==================== estruturas ==================== */

typedef struct IntList {
    int value;
    struct IntList* next;
} IntList;

typedef struct StrList {
    char* value;
    struct StrList* next;
} StrList;

typedef struct NameEntry {
    char* name;
    int   id;
    struct NameEntry* next;
} NameEntry;

typedef struct TitleEntry {
    char*    title;
    IntList* authorIds;
    struct TitleEntry* next;
} TitleEntry;

typedef struct Edge {
    int      neighborId;
    StrList* titles;
    struct Edge* next;
} Edge;

typedef struct GraphNode {
    int   id;
    Edge* edges;
} GraphNode;

/* ==================== tabelas globais ==================== */

NameEntry*  g_nameHash[NAME_HASH_SIZE];
TitleEntry* g_titleHash[TITLE_HASH_SIZE];

char**      g_index     = NULL;
GraphNode*  g_graph     = NULL;
int         g_nodeCount = 0;
int         g_capacity  = 0;

/* ==================== utilitarios ==================== */

static char* dupStr(const char* s) {
    size_t n = strlen(s) + 1;
    char*  p = (char*) malloc(n);
    if (!p) { fprintf(stderr, "erro de memoria\n"); exit(1); }
    memcpy(p, s, n);
    return p;
}

static char* trim(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char* end = s + strlen(s);
    while (end > s && (end[-1]==' '||end[-1]=='\t'||end[-1]=='\r'||end[-1]=='\n')) *--end = '\0';
    return s;
}

static void ensureCapacity(int needed) {
    if (needed <= g_capacity) return;
    int newCap = g_capacity ? g_capacity * 2 : 256;
    while (newCap < needed) newCap *= 2;
    g_index = (char**)     realloc(g_index, (size_t)newCap * sizeof(char*));
    g_graph = (GraphNode*) realloc(g_graph, (size_t)newCap * sizeof(GraphNode));
    if (!g_index || !g_graph) { fprintf(stderr, "erro de memoria\n"); exit(1); }
    g_capacity = newCap;
}

/* ==================== hashing ==================== */

unsigned int hashString(const char* s, unsigned int tableSize) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++); /* djb2 */
    return h % tableSize;
}

/* ==================== tabela de nomes ==================== */

static int findResearcherId(const char* name) {
    unsigned int h = hashString(name, NAME_HASH_SIZE);
    for (NameEntry* e = g_nameHash[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e->id;
    return -1;
}

int getOrCreateResearcherId(const char* name) {
    unsigned int h = hashString(name, NAME_HASH_SIZE);
    for (NameEntry* e = g_nameHash[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e->id;

    NameEntry* e = (NameEntry*) malloc(sizeof(NameEntry));
    e->name = dupStr(name);
    e->id   = g_nodeCount;
    e->next = g_nameHash[h];
    g_nameHash[h] = e;

    ensureCapacity(g_nodeCount + 1);
    g_index[g_nodeCount]       = e->name;
    g_graph[g_nodeCount].id    = g_nodeCount;
    g_graph[g_nodeCount].edges = NULL;
    g_nodeCount++;

    return e->id;
}

/* ==================== tabela de titulos ==================== */

void registerTitleAuthor(const char* title, int authorId) {
    unsigned int h = hashString(title, TITLE_HASH_SIZE);

    for (TitleEntry* t = g_titleHash[h]; t; t = t->next) {
        if (strcmp(t->title, title) == 0) {
            /* colisao repetida: mesmo titulo -> adiciona autor sem duplicar */
            for (IntList* a = t->authorIds; a; a = a->next)
                if (a->value == authorId) return;
            IntList* node = (IntList*) malloc(sizeof(IntList));
            node->value  = authorId;
            node->next   = t->authorIds;
            t->authorIds = node;
            return;
        }
    }

    /* colisao tradicional (ou bucket vazio): encadeia novo titulo */
    TitleEntry* t = (TitleEntry*) malloc(sizeof(TitleEntry));
    t->title            = dupStr(title);
    t->authorIds        = (IntList*) malloc(sizeof(IntList));
    t->authorIds->value = authorId;
    t->authorIds->next  = NULL;
    t->next             = g_titleHash[h];
    g_titleHash[h]      = t;
}

/* ==================== construcao do grafo ==================== */

static void addDirectedEdge(int from, int to, char* title) {
    Edge* e = g_graph[from].edges;
    while (e && e->neighborId != to) e = e->next;

    if (!e) {
        e = (Edge*) malloc(sizeof(Edge));
        e->neighborId = to;
        e->titles     = NULL;
        e->next       = g_graph[from].edges;
        g_graph[from].edges = e;
    }

    for (StrList* s = e->titles; s; s = s->next)
        if (strcmp(s->value, title) == 0) return;

    StrList* s = (StrList*) malloc(sizeof(StrList));
    s->value  = title;
    s->next   = e->titles;
    e->titles = s;
}

void connectCollaborators(void) {
    for (int b = 0; b < TITLE_HASH_SIZE; b++) {
        for (TitleEntry* t = g_titleHash[b]; t; t = t->next) {
            for (IntList* a = t->authorIds; a; a = a->next)
                for (IntList* c = a->next; c; c = c->next) {
                    addDirectedEdge(a->value, c->value, t->title);
                    addDirectedEdge(c->value, a->value, t->title);
                }
        }
    }
}

/* ==================== estatisticas de carga ==================== */

static int countUniqueTitles(void) {
    int total = 0;
    for (int b = 0; b < TITLE_HASH_SIZE; b++)
        for (TitleEntry* t = g_titleHash[b]; t; t = t->next) total++;
    return total;
}

static int countEdges(void) {
    long total = 0;
    for (int i = 0; i < g_nodeCount; i++)
        for (Edge* e = g_graph[i].edges; e; e = e->next) total++;
    return (int)(total / 2); /* cada aresta esta armazenada nas duas direcoes */
}

/* ==================== leitura do arquivo ==================== */

/* le uma linha completa de qualquer tamanho em um buffer que cresce */
static char* readFullLine(FILE* f, char** buf, size_t* cap) {
    size_t len = 0; int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= *cap) {
            *cap *= 2;
            *buf = (char*) realloc(*buf, *cap);
            if (!*buf) { fprintf(stderr, "erro de memoria\n"); exit(1); }
        }
        (*buf)[len++] = (char) c;
    }
    if (c == EOF && len == 0) return NULL;
    (*buf)[len] = '\0';
    return *buf;
}

void loadFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { perror("fopen"); exit(1); }

    /* pula BOM UTF-8 se presente */
    int c1 = fgetc(f), c2 = fgetc(f), c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }

    size_t cap = MAX_LINE;
    char* line = (char*) malloc(cap);
    if (!line) { fprintf(stderr, "erro de memoria\n"); exit(1); }
    while (readFullLine(f, &line, &cap)) {
        char* tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = '\0';
        char* name  = trim(line);
        char* title = trim(tab + 1);
        if (*name == '\0' || *title == '\0') continue;
        int id = getOrCreateResearcherId(name);
        registerTitleAuthor(title, id);
    }
    free(line);
    fclose(f);

    connectCollaborators();

    printf("Arquivo carregado.\n");
    printf("  Pesquisadores: %d\n", g_nodeCount);
    printf("  Titulos unicos: %d\n", countUniqueTitles());
    printf("  Arestas de colaboracao: %d\n", countEdges());
}

/* ==================== operacoes ==================== */

void listCollaborators(const char* name) {
    int id = findResearcherId(name);
    if (id < 0) {
        printf("Pesquisador \"%s\" nao encontrado.\n", name);
        return;
    }

    Edge* e = g_graph[id].edges;
    if (!e) {
        printf("\"%s\" nao possui colaboradores.\n", name);
        return;
    }

    printf("Colaboradores de \"%s\":\n", name);
    int count = 0;
    for (; e; e = e->next) {
        printf("  - %s\n", g_index[e->neighborId]);
        count++;
    }
    printf("Total: %d colaborador(es).\n", count);
}

void listAuthors(const char* title) {
    unsigned int h = hashString(title, TITLE_HASH_SIZE);
    /* percorrer a cadeia trata a colisao tradicional */
    TitleEntry* t = g_titleHash[h];
    while (t && strcmp(t->title, title) != 0) t = t->next;

    if (!t) {
        printf("Titulo \"%s\" nao encontrado.\n", title);
        return;
    }

    printf("Autores de \"%s\":\n", title);
    int count = 0;
    for (IntList* a = t->authorIds; a; a = a->next) {
        printf("  - %s\n", g_index[a->value]);
        count++;
    }
    printf("Total: %d autor(es).\n", count);
}

/* titulos publicados em colaboracao entre dois pesquisadores */
void listSharedTitles(const char* nameA, const char* nameB) {
    int idA = findResearcherId(nameA);
    int idB = findResearcherId(nameB);
    if (idA < 0) { printf("Pesquisador \"%s\" nao encontrado.\n", nameA); return; }
    if (idB < 0) { printf("Pesquisador \"%s\" nao encontrado.\n", nameB); return; }
    if (idA == idB) { printf("Informe dois nomes diferentes.\n"); return; }

    Edge* e = g_graph[idA].edges;
    while (e && e->neighborId != idB) e = e->next;

    if (!e) {
        printf("\"%s\" e \"%s\" nao sao colaboradores.\n", nameA, nameB);
        return;
    }

    printf("Titulos em comum entre \"%s\" e \"%s\":\n", nameA, nameB);
    int count = 0;
    for (StrList* s = e->titles; s; s = s->next) {
        printf("  - %s\n", s->value);
        count++;
    }
    printf("Total: %d titulo(s).\n", count);
}

int maxDegree(void) {
    int max = 0;
    for (int i = 0; i < g_nodeCount; i++) {
        int deg = 0;
        for (Edge* e = g_graph[i].edges; e; e = e->next) deg++;
        if (deg > max) max = deg;
    }
    return max;
}

double avgDegree(void) {
    if (g_nodeCount == 0) return 0.0;
    long total = 0;
    for (int i = 0; i < g_nodeCount; i++)
        for (Edge* e = g_graph[i].edges; e; e = e->next) total++;
    return (double)total / g_nodeCount;
}

/* ==================== liberacao de memoria ==================== */

static void freeAll(void) {
    for (int i = 0; i < NAME_HASH_SIZE; i++) {
        NameEntry* e = g_nameHash[i];
        while (e) { NameEntry* nx = e->next; free(e->name); free(e); e = nx; }
    }
    for (int i = 0; i < TITLE_HASH_SIZE; i++) {
        TitleEntry* t = g_titleHash[i];
        while (t) {
            TitleEntry* nt = t->next;
            for (IntList* a = t->authorIds; a; ) { IntList* na = a->next; free(a); a = na; }
            free(t->title);
            free(t);
            t = nt;
        }
    }
    for (int i = 0; i < g_nodeCount; i++) {
        Edge* e = g_graph[i].edges;
        while (e) {
            Edge* ne = e->next;
            for (StrList* s = e->titles; s; ) { StrList* ns = s->next; free(s); s = ns; }
            free(e);
            e = ne;
        }
    }
    free(g_index);
    free(g_graph);
}

/* ==================== main ==================== */

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s <arquivo>\n", argv[0]);
        return 1;
    }

    loadFile(argv[1]);

    int  option;
    char buf[MAX_QUERY], buf2[MAX_QUERY];
    do {
        printf("\n1) Colaboradores de um nome\n"
               "2) Autores de um titulo\n"
               "3) Maior grau\n"
               "4) Grau medio\n"
               "5) Titulos em comum entre dois pesquisadores\n"
               "0) Sair\n> ");
        if (scanf("%d", &option) != 1) break;
        getchar();

        switch (option) {
            case 1:
                printf("Nome: ");
                if (!fgets(buf, sizeof(buf), stdin)) break;
                buf[strcspn(buf, "\r\n")] = '\0';
                listCollaborators(buf);
                break;
            case 2:
                printf("Titulo: ");
                if (!fgets(buf, sizeof(buf), stdin)) break;
                buf[strcspn(buf, "\r\n")] = '\0';
                listAuthors(buf);
                break;
            case 3:
                printf("Maior grau: %d\n", maxDegree());
                break;
            case 4:
                printf("Grau medio: %.2f\n", avgDegree());
                break;
            case 5:
                printf("Nome 1: ");
                if (!fgets(buf, sizeof(buf), stdin)) break;
                buf[strcspn(buf, "\r\n")] = '\0';
                printf("Nome 2: ");
                if (!fgets(buf2, sizeof(buf2), stdin)) break;
                buf2[strcspn(buf2, "\r\n")] = '\0';
                listSharedTitles(buf, buf2);
                break;
            case 0:
                break;
            default:
                printf("Opcao invalida.\n");
        }
    } while (option != 0);

    freeAll();
    return 0;
}
