/*
Grupo PYP
Integrantes:
Pedro Augusto Faria - 821124
Pedro Yudi Teixeira Harada - 800636
Yuri Bastos Wirthmann - 812311
*/

// rm -rf build
// cmake -B build
// cmake --build build -j
// ./build/ori --ui dadosPesquisadores.txt 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAMANHO_HASH_NOME   100003
#define TAMANHO_HASH_TITULO 100003
#define TAMANHO_LINHA_MAX   4096
#define TAMANHO_CONSULTA    1024

/* ==================== estruturas ==================== */

typedef struct ListaInt {
    int valor;
    struct ListaInt* proximo;
} ListaInt;

typedef struct ListaStr {
    char* valor;
    struct ListaStr* proximo;
} ListaStr;

typedef struct EntradaNome {
    char* nome;
    int   id;
    struct EntradaNome* proximo;
} EntradaNome;

typedef struct EntradaTitulo {
    char*    titulo;
    ListaInt* idsAutores;
    struct EntradaTitulo* proximo;
} EntradaTitulo;

typedef struct Aresta {
    int      idVizinho;
    ListaStr* titulos;
    struct Aresta* proximo;
} Aresta;

typedef struct NoGrafo {
    int   id;
    Aresta* arestas;
} NoGrafo;

/* ==================== tabelas globais ==================== */

EntradaNome*  tabelaNomes[TAMANHO_HASH_NOME];
EntradaTitulo* tabelaTitulos[TAMANHO_HASH_TITULO];

char**      indiceNomes     = NULL;
NoGrafo*    grafo           = NULL;
int         quantidadeNos   = 0;
int         capacidadeAtual = 0;

/* ==================== utilitarios ==================== */

static char* dupicarString(const char* s) {
    size_t n = strlen(s) + 1;
    char*  p = (char*) malloc(n);
    if (!p) { fprintf(stderr, "erro de memoria\n"); exit(1); }
    memcpy(p, s, n);
    return p;
}

static char* removerEspacos(char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char* fim = s + strlen(s);
    while (fim > s && (fim[-1]==' '||fim[-1]=='\t'||fim[-1]=='\r'||fim[-1]=='\n')) *--fim = '\0';
    return s;
}

static void garantirCapacidade(int necessario) {
    if (necessario <= capacidadeAtual) return;
    int novaCap = capacidadeAtual ? capacidadeAtual * 2 : 256;
    while (novaCap < necessario) novaCap *= 2;
    indiceNomes = (char**)     realloc(indiceNomes, (size_t)novaCap * sizeof(char*));
    grafo       = (NoGrafo*) realloc(grafo,       (size_t)novaCap * sizeof(NoGrafo));
    if (!indiceNomes || !grafo) { fprintf(stderr, "erro de memoria\n"); exit(1); }
    capacidadeAtual = novaCap;
}

/* ==================== hashing ==================== */

unsigned int hashString(const char* s, unsigned int tamanhoTabela) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)(*s++);
    return h % tamanhoTabela;
}

/* ==================== tabela de nomes ==================== */

static int buscarIdPesquisador(const char* nome) {
    unsigned int h = hashString(nome, TAMANHO_HASH_NOME);
    for (EntradaNome* e = tabelaNomes[h]; e; e = e->proximo)
        if (strcmp(e->nome, nome) == 0) return e->id;
    return -1;
}

int obteruCriarIdPesquisador(const char* nome) {
    unsigned int h = hashString(nome, TAMANHO_HASH_NOME);
    for (EntradaNome* e = tabelaNomes[h]; e; e = e->proximo)
        if (strcmp(e->nome, nome) == 0) return e->id;

    EntradaNome* e = (EntradaNome*) malloc(sizeof(EntradaNome));
    e->nome = dupicarString(nome);
    e->id   = quantidadeNos;
    e->proximo = tabelaNomes[h];
    tabelaNomes[h] = e;

    garantirCapacidade(quantidadeNos + 1);
    indiceNomes[quantidadeNos]       = e->nome;
    grafo[quantidadeNos].id          = quantidadeNos;
    grafo[quantidadeNos].arestas     = NULL;
    quantidadeNos++;

    return e->id;
}

/* ==================== tabela de titulos ==================== */

void registrarTituloAutor(const char* titulo, int idAutor) {
    unsigned int h = hashString(titulo, TAMANHO_HASH_TITULO);

    for (EntradaTitulo* t = tabelaTitulos[h]; t; t = t->proximo) {
        if (strcmp(t->titulo, titulo) == 0) {
            for (ListaInt* a = t->idsAutores; a; a = a->proximo)
                if (a->valor == idAutor) return;
            ListaInt* no = (ListaInt*) malloc(sizeof(ListaInt));
            no->valor  = idAutor;
            no->proximo = t->idsAutores;
            t->idsAutores = no;
            return;
        }
    }

    EntradaTitulo* t = (EntradaTitulo*) malloc(sizeof(EntradaTitulo));
    t->titulo           = dupicarString(titulo);
    t->idsAutores        = (ListaInt*) malloc(sizeof(ListaInt));
    t->idsAutores->valor = idAutor;
    t->idsAutores->proximo = NULL;
    t->proximo          = tabelaTitulos[h];
    tabelaTitulos[h]    = t;
}

/* ==================== construcao do grafo ==================== */

static void adicionarArestaDirecionada(int origem, int destino, char* titulo) {
    Aresta* a = grafo[origem].arestas;
    while (a && a->idVizinho != destino) a = a->proximo;

    if (!a) {
        a = (Aresta*) malloc(sizeof(Aresta));
        a->idVizinho = destino;
        a->titulos   = NULL;
        a->proximo   = grafo[origem].arestas;
        grafo[origem].arestas = a;
    }

    for (ListaStr* s = a->titulos; s; s = s->proximo)
        if (strcmp(s->valor, titulo) == 0) return;

    ListaStr* s = (ListaStr*) malloc(sizeof(ListaStr));
    s->valor  = titulo;
    s->proximo = a->titulos;
    a->titulos = s;
}

void conectarColaboradores(void) {
    for (int b = 0; b < TAMANHO_HASH_TITULO; b++) {
        for (EntradaTitulo* t = tabelaTitulos[b]; t; t = t->proximo) {
            for (ListaInt* a = t->idsAutores; a; a = a->proximo)
                for (ListaInt* c = a->proximo; c; c = c->proximo) {
                    adicionarArestaDirecionada(a->valor, c->valor, t->titulo);
                    adicionarArestaDirecionada(c->valor, a->valor, t->titulo);
                }
        }
    }
}

/* ==================== estatisticas ==================== */

static int contarTitulosUnicos(void) {
    int total = 0;
    for (int b = 0; b < TAMANHO_HASH_TITULO; b++)
        for (EntradaTitulo* t = tabelaTitulos[b]; t; t = t->proximo) total++;
    return total;
}

static int contarArestas(void) {
    long total = 0;
    for (int i = 0; i < quantidadeNos; i++)
        for (Aresta* a = grafo[i].arestas; a; a = a->proximo) total++;
    return (int)(total / 2);
}

/* ==================== leitura do arquivo ==================== */

static char* lerLinhaCompleta(FILE* f, char** buffer, size_t* cap) {
    size_t len = 0; int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= *cap) {
            *cap *= 2;
            *buffer = (char*) realloc(*buffer, *cap);
            if (!*buffer) { fprintf(stderr, "erro de memoria\n"); exit(1); }
        }
        (*buffer)[len++] = (char) c;
    }
    if (c == EOF && len == 0) return NULL;
    (*buffer)[len] = '\0';
    return *buffer;
}

void carregarArquivo(const char* caminho) {
    FILE* f = fopen(caminho, "r");
    if (!f) { perror("fopen"); exit(1); }

    int c1 = fgetc(f), c2 = fgetc(f), c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }

    size_t cap = TAMANHO_LINHA_MAX;
    char* linha = (char*) malloc(cap);
    if (!linha) { fprintf(stderr, "erro de memoria\n"); exit(1); }
    while (lerLinhaCompleta(f, &linha, &cap)) {
        char* tab = strchr(linha, '\t');
        if (!tab) continue;
        *tab = '\0';
        char* nome  = removerEspacos(linha);
        char* titulo = removerEspacos(tab + 1);
        if (*nome == '\0' || *titulo == '\0') continue;
        int id = obteruCriarIdPesquisador(nome);
        registrarTituloAutor(titulo, id);
    }
    free(linha);
    fclose(f);

    conectarColaboradores();

    printf("Arquivo carregado.\n");
    printf("  Pesquisadores: %d\n", quantidadeNos);
    printf("  Titulos unicos: %d\n", contarTitulosUnicos());
    printf("  Arestas de colaboracao: %d\n", contarArestas());
}

/* ==================== operacoes ==================== */

void listarColaboradores(const char* nome) {
    int id = buscarIdPesquisador(nome);
    if (id < 0) {
        printf("Pesquisador \"%s\" nao encontrado.\n", nome);
        return;
    }

    Aresta* a = grafo[id].arestas;
    if (!a) {
        printf("\"%s\" nao possui colaboradores.\n", nome);
        return;
    }

    printf("Colaboradores de \"%s\":\n", nome);
    int cont = 0;
    for (; a; a = a->proximo) {
        printf("  - %s\n", indiceNomes[a->idVizinho]);
        cont++;
    }
    printf("Total: %d colaborador(es).\n", cont);
}

void listarAutores(const char* titulo) {
    unsigned int h = hashString(titulo, TAMANHO_HASH_TITULO);
    EntradaTitulo* t = tabelaTitulos[h];
    while (t && strcmp(t->titulo, titulo) != 0) t = t->proximo;

    if (!t) {
        printf("Titulo \"%s\" nao encontrado.\n", titulo);
        return;
    }

    printf("Autores de \"%s\":\n", titulo);
    int cont = 0;
    for (ListaInt* a = t->idsAutores; a; a = a->proximo) {
        printf("  - %s\n", indiceNomes[a->valor]);
        cont++;
    }
    printf("Total: %d autor(es).\n", cont);
}

void listarTitulosComuns(const char* nomeA, const char* nomeB) {
    int idA = buscarIdPesquisador(nomeA);
    int idB = buscarIdPesquisador(nomeB);
    if (idA < 0) { printf("Pesquisador \"%s\" nao encontrado.\n", nomeA); return; }
    if (idB < 0) { printf("Pesquisador \"%s\" nao encontrado.\n", nomeB); return; }
    if (idA == idB) { printf("Informe dois nomes diferentes.\n"); return; }

    Aresta* a = grafo[idA].arestas;
    while (a && a->idVizinho != idB) a = a->proximo;

    if (!a) {
        printf("\"%s\" e \"%s\" nao sao colaboradores.\n", nomeA, nomeB);
        return;
    }

    printf("Titulos em comum entre \"%s\" e \"%s\":\n", nomeA, nomeB);
    int cont = 0;
    for (ListaStr* s = a->titulos; s; s = s->proximo) {
        printf("  - %s\n", s->valor);
        cont++;
    }
    printf("Total: %d titulo(s).\n", cont);
}

int grauMaximo(void) {
    int max = 0;
    for (int i = 0; i < quantidadeNos; i++) {
        int deg = 0;
        for (Aresta* a = grafo[i].arestas; a; a = a->proximo) deg++;
        if (deg > max) max = deg;
    }
    return max;
}

double grauMedio(void) {
    if (quantidadeNos == 0) return 0.0;
    long total = 0;
    for (int i = 0; i < quantidadeNos; i++)
        for (Aresta* a = grafo[i].arestas; a; a = a->proximo) total++;
    return (double)total / quantidadeNos;
}

/* ==================== liberacao de memoria ==================== */

static void liberarTudo(void) {
    for (int i = 0; i < TAMANHO_HASH_NOME; i++) {
        EntradaNome* e = tabelaNomes[i];
        while (e) { EntradaNome* prox = e->proximo; free(e->nome); free(e); e = prox; }
    }
    for (int i = 0; i < TAMANHO_HASH_TITULO; i++) {
        EntradaTitulo* t = tabelaTitulos[i];
        while (t) {
            EntradaTitulo* prox = t->proximo;
            for (ListaInt* a = t->idsAutores; a; ) { ListaInt* pa = a->proximo; free(a); a = pa; }
            free(t->titulo);
            free(t);
            t = prox;
        }
    }
    for (int i = 0; i < quantidadeNos; i++) {
        Aresta* a = grafo[i].arestas;
        while (a) {
            Aresta* pa = a->proximo;
            for (ListaStr* s = a->titulos; s; ) { ListaStr* ps = s->proximo; free(s); s = ps; }
            free(a);
            a = pa;
        }
    }
    free(indiceNomes);
    free(grafo);
}

/* ==================== main ==================== */

int main(int argc, char** argv) {

    if (argc < 2) {
        fprintf(stderr, "uso: %s <arquivo>\n", argv[0]);
        return 1;
    }

    carregarArquivo(argv[1]);

    int  opcao;
    char buf[TAMANHO_CONSULTA], buf2[TAMANHO_CONSULTA];
    
    do {
        printf("\n1) Colaboradores de um nome\n"
               "2) Autores de um titulo\n"
               "3) Maior grau\n"
               "4) Grau medio\n"
               "5) Titulos em comum entre dois pesquisadores\n"
               "0) Sair\n> ");
        if (scanf("%d", &opcao) != 1) break;
        getchar();

        switch (opcao) {
            case 1:
                printf("Nome: ");
                if (!fgets(buf, sizeof(buf), stdin)) break;
                buf[strcspn(buf, "\r\n")] = '\0';
                listarColaboradores(buf);
                break;
            case 2:
                printf("Titulo: ");
                if (!fgets(buf, sizeof(buf), stdin)) break;
                buf[strcspn(buf, "\r\n")] = '\0';
                listarAutores(buf);
                break;
            case 3:
                printf("Maior grau: %d\n", grauMaximo());
                break;
            case 4:
                printf("Grau medio: %.2f\n", grauMedio());
                break;
            case 5:
                printf("Nome 1: ");
                if (!fgets(buf, sizeof(buf), stdin)) break;
                buf[strcspn(buf, "\r\n")] = '\0';
                printf("Nome 2: ");
                if (!fgets(buf2, sizeof(buf2), stdin)) break;
                buf2[strcspn(buf2, "\r\n")] = '\0';
                listarTitulosComuns(buf, buf2);
                break;
            case 0:
                break;
            default:
                printf("Opcao invalida.\n");
        }
    } while (opcao != 0);

    liberarTudo();
    return 0;
}