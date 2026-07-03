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

/* lista de inteiros: ids dos autores de um titulo */
typedef struct IntList {
    int value;
    struct IntList* next;
} IntList;

/* lista de strings: titulos em comum de uma aresta */
typedef struct StrList {
    char* value;                /* aponta para o titulo guardado no TitleEntry */
    struct StrList* next;
} StrList;

/* entrada da hash de nomes (nome -> id do no) */
typedef struct NameEntry {
    char* name;
    int   id;
    struct NameEntry* next;     /* encadeamento de colisoes */
} NameEntry;

/* entrada da hash de titulos (titulo -> autores) */
typedef struct TitleEntry {
    char*    title;
    IntList* authorIds;         /* ids de todos os autores deste titulo */
    struct TitleEntry* next;    /* encadeamento de colisoes tradicionais */
} TitleEntry;

/* aresta do grafo de colaboracoes */
typedef struct Edge {
    int      neighborId;
    StrList* titles;            /* titulos publicados em colaboracao */
    struct Edge* next;
} Edge;

/* no do grafo */
typedef struct GraphNode {
    int   id;
    Edge* edges;
} GraphNode;

/* ==================== tabelas globais ==================== */

NameEntry*  g_nameHash[NAME_HASH_SIZE];
TitleEntry* g_titleHash[TITLE_HASH_SIZE];

char**      g_index     = NULL;   /* id -> nome (indice) */
GraphNode*  g_graph     = NULL;   /* id -> no do grafo   */
int         g_nodeCount = 0;
int         g_capacity  = 0;

/* ==================== utilitarios ==================== */

static char* dupStr(const char* s) {
    size_t n = strlen(s) + 1;
    char*  p = malloc(n);
    if (!p) { fprintf(stderr, "erro de memoria\n"); exit(1); }
    memcpy(p, s, n);
    return p;
}

/* remove espacos e quebras de linha no inicio e no fim */
static char* trim(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') return s;
    char* end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';
    return s;
}

/* garante espaco no indice e no grafo para 'needed' nos */
static void ensureCapacity(int needed) {
    if (needed <= g_capacity) return;
    int newCap = g_capacity ? g_capacity * 2 : 256;
    while (newCap < needed) newCap *= 2;
    g_index = realloc(g_index, (size_t)newCap * sizeof(char*));
    g_graph = realloc(g_graph, (size_t)newCap * sizeof(GraphNode));
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

/* procura um nome; retorna -1 se nao existir */
static int findResearcherId(const char* name) {
    unsigned int h = hashString(name, NAME_HASH_SIZE);
    for (NameEntry* e = g_nameHash[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e->id;
    return -1;
}

/* procura o nome; se nao existir, cria um novo no no grafo */
int getOrCreateResearcherId(const char* name) {
    unsigned int h = hashString(name, NAME_HASH_SIZE);
    for (NameEntry* e = g_nameHash[h]; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e->id;

    NameEntry* e = malloc(sizeof(NameEntry));
    e->name = dupStr(name);
    e->id   = g_nodeCount;
    e->next = g_nameHash[h];
    g_nameHash[h] = e;

    ensureCapacity(g_nodeCount + 1);
    g_index[g_nodeCount]       = e->name;   /* mesmo ponteiro do NameEntry */
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
            IntList* node = malloc(sizeof(IntList));
            node->value  = authorId;
            node->next   = t->authorIds;
            t->authorIds = node;
            return;
        }
    }

    /* titulo novo (bucket vazio ou colisao tradicional) -> encadeia */
    TitleEntry* t = malloc(sizeof(TitleEntry));
    t->title            = dupStr(title);
    t->authorIds        = malloc(sizeof(IntList));
    t->authorIds->value = authorId;
    t->authorIds->next  = NULL;
    t->next             = g_titleHash[h];
    g_titleHash[h]      = t;
}

/* ==================== construcao do grafo ==================== */

/* cria/atualiza a aresta direcionada 'from' -> 'to' com o titulo em comum */
static void addDirectedEdge(int from, int to, char* title) {
    Edge* e = g_graph[from].edges;
    while (e && e->neighborId != to) e = e->next;

    if (!e) {                       /* ainda nao eram colaboradores */
        e = malloc(sizeof(Edge));
        e->neighborId = to;
        e->titles     = NULL;
        e->next       = g_graph[from].edges;
        g_graph[from].edges = e;
    }

    /* evita repetir o mesmo titulo na aresta */
    for (StrList* s = e->titles; s; s = s->next)
        if (strcmp(s->value, title) == 0) return;

    StrList* s = malloc(sizeof(StrList));
    s->value  = title;              /* aponta para o titulo do TitleEntry */
    s->next   = e->titles;
    e->titles = s;
}

/* liga todos os pares de autores que compartilham um mesmo titulo */
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

/* ==================== leitura do arquivo ==================== */

void loadFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { perror("fopen"); exit(1); }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        char* tab = strchr(line, '\t');
        if (!tab) continue;              /* linha sem separador: ignora */
        *tab = '\0';

        char* name  = trim(line);
        char* title = trim(tab + 1);
        if (*name == '\0' || *title == '\0') continue;

        int id = getOrCreateResearcherId(name);
        registerTitleAuthor(title, id);
    }
    fclose(f);

    connectCollaborators();
}

/* ==================== operacoes ==================== */

/* dado um nome, lista seus colaboradores */
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

/* dado um titulo, lista todos os autores */
void listAuthors(const char* title) {
    unsigned int h = hashString(title, TITLE_HASH_SIZE);
    TitleEntry* t = g_titleHash[h];
    while (t && strcmp(t->title, title) != 0) t = t->next;

    if (!t) {
        printf("Titulo \"%s\" nao encontrado.\n", title);
        return;
    }

    printf("Autores de \"%s\":\n", title);
    for (IntList* a = t->authorIds; a; a = a->next)
        printf("  - %s\n", g_index[a->value]);
}

/* maior grau (numero de colaboradores) entre os vertices */
int maxDegree(void) {
    int max = 0;
    for (int i = 0; i < g_nodeCount; i++) {
        int deg = 0;
        for (Edge* e = g_graph[i].edges; e; e = e->next) deg++;
        if (deg > max) max = deg;
    }
    return max;
}

/* grau medio = soma dos graus / numero de vertices */
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
    char buf[MAX_QUERY];
    do {
        printf("\n1) Colaboradores de um nome\n"
               "2) Autores de um titulo\n"
               "3) Maior grau\n"
               "4) Grau medio\n"
               "0) Sair\n> ");
        if (scanf("%d", &option) != 1) break;
        getchar();  /* consome o '\n' deixado pelo scanf */

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
            case 0:
                break;
            default:
                printf("Opcao invalida.\n");
        }
    } while (option != 0);

    freeAll();
    return 0;
}