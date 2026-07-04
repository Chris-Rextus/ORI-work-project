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

/* ---- bibliotecas para a interface grafica (adicionado) ---- */
#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <functional>


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

/* ==================== interface grafica em Qt (adicionado) ==================== */
/* Nenhuma classe aqui usa Q_OBJECT -> nao ha moc: tudo fica num unico arquivo.
   As tabelas globais e as funcoes ja existentes sao reutilizadas sem alteracao. */
namespace ui {

const double K_SPRING      = 150.0;   /* comprimento ideal das arestas */
const int    MAX_NEIGHBORS = 35;     /* limite de vizinhos exibidos por foco */
const int    LAYOUT_ITERS  = 900;    /* teto de iteracoes do layout */

class GraphView;   /* declaracao adiantada (State guarda um ponteiro) */

struct LNode {
    int id = -1;
    double x = 0, y = 0, dx = 0, dy = 0, vx = 0, vy = 0;
    int degree = 0;
    QGraphicsEllipseItem*    dot   = nullptr;
    QGraphicsSimpleTextItem* label = nullptr;
};

struct LEdge {
    int a = 0, b = 0;                 /* indices em State::nodes */
    QGraphicsLineItem* line = nullptr;
};

struct State {
    QGraphicsScene scene;
    GraphView*   view      = nullptr;
    QLineEdit*   search    = nullptr;
    QLabel*      stats     = nullptr;
    QLabel*      focusInfo = nullptr;
    QListWidget* neighbors = nullptr;
    QTimer       timer;

    std::vector<LNode> nodes;
    std::vector<LEdge> edges;
    std::unordered_map<int,int> index;   /* id do pesquisador -> posicao em nodes */

    int    center       = -1;
    int    maxDegGlobal = 1;
    int    iter         = 0;
    double temp         = 0.0;
};

int nodeDegree(int id) {
    int d = 0;
    for (Edge* e = g_graph[id].edges; e; e = e->next) d++;
    return d;
}

QString sharedTitlesText(int u, int v) {
    for (Edge* e = g_graph[u].edges; e; e = e->next) {
        if (e->neighborId == v) {
            QString out; int n = 0;
            for (StrList* s = e->titles; s; s = s->next) {
                if (n < 8) { out += "\u2022 "; out += QString::fromUtf8(s->value); out += "\n"; }
                n++;
            }
            if (n > 8) out += QString("\u2026 (+%1)").arg(n - 8);
            return QString("%1 titulo(s) em comum:\n%2").arg(n).arg(out);
        }
    }
    return QString();
}

QColor colorForDegree(int deg, int maxDeg) {
    double t = maxDeg > 0 ? (double)deg / (double)maxDeg : 0.0;
    if (t > 1.0) t = 1.0;
    t = std::sqrt(t);
    int r = (int)(150 + t * (40  - 150));
    int g = (int)(160 + t * (90  - 160));
    int b = (int)(175 + t * (150 - 175));
    return QColor(r, g, b);
}

class GraphView : public QGraphicsView {
public:
    std::function<void(int)> onActivate;
    explicit GraphView(QGraphicsScene* s) : QGraphicsView(s) {
        setRenderHint(QPainter::Antialiasing, true);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setBackgroundBrush(QColor(255, 255, 255));
    }
protected:
    void wheelEvent(QWheelEvent* ev) override {
        double f = (ev->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        scale(f, f);
    }
    void mouseDoubleClickEvent(QMouseEvent* ev) override {
        QGraphicsItem* it = itemAt(ev->position().toPoint());
        if (it) {
            QVariant v = it->data(0);
            if (v.isValid() && onActivate) { onActivate(v.toInt()); return; }
        }
        QGraphicsView::mouseDoubleClickEvent(ev);
    }
};

void buildEgo(State& st, int centerId) {
    st.timer.stop();
    st.nodes.clear();
    st.edges.clear();
    st.index.clear();
    st.scene.clear();
    st.center = centerId;

    std::vector<int> nb;
    for (Edge* e = g_graph[centerId].edges; e; e = e->next) nb.push_back(e->neighborId);
    std::sort(nb.begin(), nb.end(), [](int x, int y){ return nodeDegree(x) > nodeDegree(y); });
    if ((int)nb.size() > MAX_NEIGHBORS) nb.resize(MAX_NEIGHBORS);

    auto addNode = [&](int id){
        if (st.index.count(id)) return;
        LNode n;
        n.id = id;
        n.degree = nodeDegree(id);
        double ang = QRandomGenerator::global()->generateDouble() * 6.2831853;
        double rad = 80.0 + QRandomGenerator::global()->generateDouble() * 420.0;
        n.x = std::cos(ang) * rad;
        n.y = std::sin(ang) * rad;
        st.index[id] = (int)st.nodes.size();
        st.nodes.push_back(n);
    };
    addNode(centerId);
    st.nodes[0].x = 0; st.nodes[0].y = 0;
    for (int id : nb) addNode(id);

    for (size_t i = 0; i < st.nodes.size(); i++) {
        int u = st.nodes[i].id;
        for (Edge* e = g_graph[u].edges; e; e = e->next) {
            auto it = st.index.find(e->neighborId);
            if (it != st.index.end() && it->second > (int)i) {
                LEdge le; le.a = (int)i; le.b = it->second;
                st.edges.push_back(le);
            }
        }
    }

    for (auto& le : st.edges) {
        QGraphicsLineItem* line = st.scene.addLine(0,0,0,0, QPen(QColor(0, 0, 0, 90), 0.8));
        line->setZValue(-1);
        line->setAcceptedMouseButtons(Qt::NoButton);
        line->setToolTip(sharedTitlesText(st.nodes[le.a].id, st.nodes[le.b].id));
        le.line = line;
    }

    for (size_t i = 0; i < st.nodes.size(); i++) {
        LNode& n = st.nodes[i];
        double r = 4.0 + 1.4 * std::sqrt((double)n.degree);
        if (r > 14.0) r = 14.0;
        QColor col = (n.id == centerId) ? QColor(230, 90, 70)
                                        : colorForDegree(n.degree, st.maxDegGlobal);
        QGraphicsEllipseItem* dot = st.scene.addEllipse(-r, -r, 2*r, 2*r,
                                        QPen(Qt::NoPen), QBrush(col));
        dot->setZValue(1);
        dot->setAcceptedMouseButtons(Qt::NoButton);
        dot->setData(0, n.id);
        dot->setToolTip(QString("%1  (grau %2)")
                        .arg(QString::fromUtf8(g_index[n.id])).arg(n.degree));
        n.dot = dot;

        QGraphicsSimpleTextItem* lab = st.scene.addSimpleText(QString::fromUtf8(g_index[n.id]));
        lab->setBrush(QColor(20, 20, 20));
        lab->setZValue(2);
        lab->setAcceptedMouseButtons(Qt::NoButton);
        lab->setData(0, n.id);
        /* so o centro e os vizinhos de maior grau recebem rotulo -> menos poluicao */
        lab->setVisible(n.id == centerId || n.degree >= 3);
        QFont f("Sans Serif", 0, QFont::Light);
        f.setPointSizeF(n.id == centerId ? 9.0 : 6.5);
        f.setLetterSpacing(QFont::PercentageSpacing, 96);
        lab->setFont(f);
        n.label = lab;
    }

    st.iter = 0;
    st.temp = 60.0;
    st.timer.start(1000 / 60);
    if (st.view)
        st.view->fitInView(st.scene.itemsBoundingRect().adjusted(-60,-60,60,60),
                           Qt::KeepAspectRatio);
}

void applyLayout(State& st) {
    const int n = (int)st.nodes.size();
    if (n == 0) { st.timer.stop(); return; }

    for (int i = 0; i < n; i++) { st.nodes[i].dx = 0; st.nodes[i].dy = 0; }

    /* repulsao entre todos os pares, com distancia minima para nao explodir */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            double ex = st.nodes[i].x - st.nodes[j].x;
            double ey = st.nodes[i].y - st.nodes[j].y;
            double d = std::sqrt(ex*ex + ey*ey);
            if (d < 1.0) {   /* evita divisao por ~0 e empurra em direcao estavel */
                ex = (i - j); ey = 1.0;
                d = std::sqrt(ex*ex + ey*ey);
            }
            double force = (K_SPRING * K_SPRING) / d;
            if (force > 1000.0) force = 1000.0;   /* teto: nada de saltos violentos */
            double fx = ex / d * force, fy = ey / d * force;
            st.nodes[i].dx += fx; st.nodes[i].dy += fy;
            st.nodes[j].dx -= fx; st.nodes[j].dy -= fy;
        }

    /* atracao ao longo das arestas */
    for (auto& le : st.edges) {
        double ex = st.nodes[le.a].x - st.nodes[le.b].x;
        double ey = st.nodes[le.a].y - st.nodes[le.b].y;
        double d = std::sqrt(ex*ex + ey*ey) + 0.01;
        double force = (d * d) / K_SPRING;
        double fx = ex / d * force, fy = ey / d * force;
        st.nodes[le.a].dx -= fx; st.nodes[le.a].dy -= fy;
        st.nodes[le.b].dx += fx; st.nodes[le.b].dy += fy;
    }

    /* leve gravidade para o centro, mantem o grafo coeso */
    for (int i = 0; i < n; i++) {
        st.nodes[i].dx += -st.nodes[i].x * 0.03;
        st.nodes[i].dy += -st.nodes[i].y * 0.03;
    }

    /* integra com velocidade amortecida: evita o vai-e-vem entre duas posicoes */
    const double DAMPING  = 0.85;   /* atrito: quanto da velocidade sobrevive por frame */
    const double MAX_STEP = 8.0;    /* teto de seguranca, agora raramente ativado */
    double totalMovement = 0.0;
    for (int i = 0; i < n; i++) {
        if (st.nodes[i].id == st.center) continue;   /* centro fixo na origem */

        /* acumula forca na velocidade e aplica atrito -> convergencia suave */
        st.nodes[i].vx = (st.nodes[i].vx + st.nodes[i].dx * 0.05) * DAMPING;
        st.nodes[i].vy = (st.nodes[i].vy + st.nodes[i].dy * 0.05) * DAMPING;

        double len = std::sqrt(st.nodes[i].vx*st.nodes[i].vx + st.nodes[i].vy*st.nodes[i].vy);
        if (len > st.temp) { st.nodes[i].vx *= st.temp/len; st.nodes[i].vy *= st.temp/len; len = st.temp; }
        if (len > MAX_STEP){ st.nodes[i].vx *= MAX_STEP/len; st.nodes[i].vy *= MAX_STEP/len; len = MAX_STEP; }

        st.nodes[i].x += st.nodes[i].vx;
        st.nodes[i].y += st.nodes[i].vy;
        totalMovement += len;
    }

    for (auto& le : st.edges)
        le.line->setLine(st.nodes[le.a].x, st.nodes[le.a].y,
                         st.nodes[le.b].x, st.nodes[le.b].y);
    for (auto& nd : st.nodes) {
        nd.dot->setPos(nd.x, nd.y);
        QRectF br = nd.label->boundingRect();
        double r = 4.0 + 1.4 * std::sqrt((double)nd.degree);
        if (r > 14.0) r = 14.0;
        nd.label->setPos(nd.x - br.width() / 2.0, nd.y - r - br.height() - 4.0);
    }

    st.temp *= 0.99;                 /* resfriamento gradual */
    st.iter++;

    /* media de deslocamento por no neste frame */
    double avgMovement = (n > 1) ? totalMovement / (n - 1) : 0.0;

    /* para assim que o grafo estabiliza (quase sem movimento) ou atinge o teto */
    if (avgMovement < 0.15 || st.iter >= LAYOUT_ITERS) {
        st.timer.stop();
        if (st.view)                 /* enquadra apenas UMA vez, no fim */
            st.view->fitInView(st.scene.itemsBoundingRect().adjusted(-60,-60,60,60),
                               Qt::KeepAspectRatio);
    }
}

void refreshPanel(State& st) {
    int id = st.center;
    st.focusInfo->setText(
        QString("<b>%1</b><br>grau: %2 &nbsp;&bull;&nbsp; exibindo %3 vizinho(s)")
        .arg(QString::fromUtf8(g_index[id]).toHtmlEscaped())
        .arg(nodeDegree(id))
        .arg((int)st.nodes.size() - 1));

    st.neighbors->clear();
    std::vector<int> nb;
    for (Edge* e = g_graph[id].edges; e; e = e->next) nb.push_back(e->neighborId);
    std::sort(nb.begin(), nb.end(), [](int a, int b){ return nodeDegree(a) > nodeDegree(b); });
    for (int v : nb) {
        QListWidgetItem* it = new QListWidgetItem(
            QString("%1  (%2)").arg(QString::fromUtf8(g_index[v])).arg(nodeDegree(v)));
        it->setData(Qt::UserRole, v);
        st.neighbors->addItem(it);
    }
}

void focus(State& st, int id) {
    if (id < 0 || id >= g_nodeCount) return;
    buildEgo(st, id);
    refreshPanel(st);
    if (st.search) st.search->setText(QString::fromUtf8(g_index[id]));
}

int runUi(int argc, char** argv, const char* path) {
    QApplication app(argc, argv);

    loadFile(path);   /* reutiliza a carga ja existente (popula as tabelas globais) */

    State* st = new State();
    st->maxDegGlobal = maxDegree();
    if (st->maxDegGlobal < 1) st->maxDegGlobal = 1;

    QWidget* window = new QWidget();
    window->setWindowTitle("ORI \u2014 Grafo de Colaboracoes");
    window->setWindowIcon(QIcon("resources/logo.jpg"));
    window->resize(1180, 760);

    st->view = new GraphView(&st->scene);
    st->view->onActivate = [st](int id){ focus(*st, id); };

    QWidget* side = new QWidget();
    side->setFixedWidth(300);
    QVBoxLayout* sideLay = new QVBoxLayout(side);

    QLabel* title = new QLabel("<h3>Grafo de Colaboracoes</h3>");
    st->stats = new QLabel();
    st->stats->setTextFormat(Qt::RichText);
    st->stats->setText(QString(
        "Pesquisadores: <b>%1</b><br>"
        "Arestas: <b>%2</b><br>"
        "Maior grau: <b>%3</b><br>"
        "Grau medio: <b>%4</b>")
        .arg(g_nodeCount).arg(countEdges()).arg(maxDegree())
        .arg(avgDegree(), 0, 'f', 2));

    QLabel* searchLbl = new QLabel("Pesquisador:");
    st->search = new QLineEdit();
    st->search->setPlaceholderText("digite um nome e Enter");
    QPushButton* btn = new QPushButton("Focar");

    st->focusInfo = new QLabel();
    st->focusInfo->setTextFormat(Qt::RichText);
    st->focusInfo->setWordWrap(true);

    QLabel* nbLbl = new QLabel("Colaboradores (clique para focar):");
    st->neighbors = new QListWidget();

    QLabel* hint = new QLabel(
        "<i>Duplo-clique num no para focar.<br>Roda = zoom &nbsp; arraste = mover.</i>");
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#888;");

    sideLay->addWidget(title);
    sideLay->addWidget(st->stats);
    sideLay->addSpacing(8);
    sideLay->addWidget(searchLbl);
    sideLay->addWidget(st->search);
    sideLay->addWidget(btn);
    sideLay->addSpacing(8);
    sideLay->addWidget(st->focusInfo);
    sideLay->addWidget(nbLbl);
    sideLay->addWidget(st->neighbors, 1);
    sideLay->addWidget(hint);

    QHBoxLayout* root = new QHBoxLayout(window);
    root->addWidget(st->view, 1);
    root->addWidget(side);

    auto doSearch = [st]() {
        QByteArray name = st->search->text().trimmed().toUtf8();
        int id = findResearcherId(name.constData());
        if (id < 0) {
            st->focusInfo->setText(
                "<span style='color:#e66;'>Pesquisador nao encontrado.</span>");
            return;
        }
        focus(*st, id);
    };
    QObject::connect(st->search, &QLineEdit::returnPressed, doSearch);
    QObject::connect(btn, &QPushButton::clicked, doSearch);
    QObject::connect(st->neighbors, &QListWidget::itemClicked,
                     [st](QListWidgetItem* it){ focus(*st, it->data(Qt::UserRole).toInt()); });
    QObject::connect(&st->timer, &QTimer::timeout, [st]{ applyLayout(*st); });

    int start = 0, best = -1;
    for (int i = 0; i < g_nodeCount; i++) {
        int d = nodeDegree(i);
        if (d > best) { best = d; start = i; }
    }
    focus(*st, start);   /* foco inicial: pesquisador de maior grau */

    window->show();
    st->view->fitInView(st->scene.itemsBoundingRect().adjusted(-60,-60,60,60),
                        Qt::KeepAspectRatio);
    int rc = app.exec();
    freeAll();   /* mesma limpeza usada pelo console */
    return rc;
}

} /* namespace ui */

/* ==================== main ==================== */

int main(int argc, char** argv) {

    if (argc < 2) {
        fprintf(stderr, "uso: %s <arquivo>\n", argv[0]);
        return 1;
    }

    /* ---- modo interface grafica (adicionado): ./ori --ui <arquivo> ---- */
    {
        bool useUi = false;
        const char* uiPath = NULL;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--ui") == 0 || strcmp(argv[i], "--gui") == 0)
                useUi = true;
            else if (uiPath == NULL)
                uiPath = argv[i];
        }
        if (useUi) {
            if (!uiPath) {
                fprintf(stderr, "uso: %s --ui <arquivo>\n", argv[0]);
                return 1;
            }
            return ui::runUi(argc, argv, uiPath);
        }
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
