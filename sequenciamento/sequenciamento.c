#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Ajuste de limites para eficiência de memória
#define MAX_NODES 2000000 
#define MAX_GENES 30000   

typedef struct {
    int id_gene;
    int next;
} NodeLista;

// Estrutura agrupada para favorecer o Cache L1/L2
typedef struct {
    int next[4];
    int fail;
    int head_gene;
} TrieNode;

TrieNode *nodes;
int nodes_count = 1;

NodeLista *pool;
int pool_ptr = 1;

int mapa_base[256];
char *gene_found;

// Buffer para leitura ultra-rápida de arquivos
char *buffer_arq;
long pos_buf = 0;
long len_buf = 0;

void setup_mapa() {
    for(int i = 0; i < 256; i++) mapa_base[i] = -1;
    mapa_base['A'] = 0; mapa_base['C'] = 1; 
    mapa_base['G'] = 2; mapa_base['T'] = 3;
}

// Inserção na Trie para múltiplos padrões
void insert(char *s, int id_global) {
    int u = 0;
    for (int i = 0; s[i]; i++) {
        int c = mapa_base[(unsigned char)s[i]];
        if (c == -1) continue;
        if (!nodes[u].next[c]) {
            nodes[u].next[c] = nodes_count++;
        }
        u = nodes[u].next[c];
    }
    int p = pool_ptr++;
    pool[p].id_gene = id_global;
    pool[p].next = nodes[u].head_gene;
    nodes[u].head_gene = p;
}

int *q_bfs;
// Construção das falhas do algoritmo Aho-Corasick
void build_ac() {
    int h = 0, t = 0;
    for (int i = 0; i < 4; i++) {
        if (nodes[0].next[i]) q_bfs[t++] = nodes[0].next[i];
    }
    
    while (h < t) {
        int u = q_bfs[h++];
        for (int i = 0; i < 4; i++) {
            int v = nodes[u].next[i];
            if (v) {
                nodes[v].fail = nodes[nodes[u].fail].next[i];
                q_bfs[t++] = v; 
            } else {
                nodes[u].next[i] = nodes[nodes[u].fail].next[i];
            }
        }
    }
}

typedef struct {
    char codigo[50];
    int qtd_genes;
    int *gene_ids;
    int percentual;
    int id_orig;
} Doenca;

// Ordenação estável: prioridade ao percentual, depois à ordem de entrada
void ordenar(Doenca *d, int n) {
    for (int i = 0; i < n - 1; i++) {
        int trocou = 0;
        for (int j = 0; j < n - i - 1; j++) {
            int troca = 0;
            if (d[j].percentual < d[j+1].percentual) troca = 1;
            else if (d[j].percentual == d[j+1].percentual && d[j].id_orig > d[j+1].id_orig) troca = 1;
            if (troca) {
                Doenca tmp = d[j]; d[j] = d[j+1]; d[j+1] = tmp;
                trocou = 1;
            }
        }
        if (!trocou) break;
    }
}

int fast_read_int() {
    int x = 0;
    while (pos_buf < len_buf && buffer_arq[pos_buf] <= 32) pos_buf++;
    while (pos_buf < len_buf && buffer_arq[pos_buf] > 32) {
        x = (x * 10) + (buffer_arq[pos_buf] - '0');
        pos_buf++;
    }
    return x;
}

int main(int argc, char *argv[]) {
    setup_mapa();
    
    nodes = calloc(MAX_NODES, sizeof(TrieNode));
    pool = malloc(sizeof(NodeLista) * (MAX_GENES + 100));
    q_bfs = malloc(sizeof(int) * MAX_NODES);
    char *visited = calloc(MAX_NODES, sizeof(char));
    
    char *in_path = (argc > 1) ? argv[1] : "sequenciamento.input.txt";
    FILE *f = fopen(in_path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    len_buf = ftell(f);
    rewind(f);
    buffer_arq = malloc(len_buf + 1);
    if(fread(buffer_arq, 1, len_buf, f) != (size_t)len_buf) { fclose(f); return 1; }
    buffer_arq[len_buf] = '\0';
    fclose(f);

    fast_read_int(); // Pula o tamanho da subcadeia (não utilizado nesta abordagem)
    
    while (pos_buf < len_buf && buffer_arq[pos_buf] <= 32) pos_buf++;
    char *dna_ptr = &buffer_arq[pos_buf];
    while (pos_buf < len_buf && buffer_arq[pos_buf] > 32) pos_buf++;
    buffer_arq[pos_buf] = '\0'; // Finaliza a string do DNA no buffer
    pos_buf++;

    int qtd_doencas = fast_read_int();
    Doenca *lista_doencas = malloc(sizeof(Doenca) * qtd_doencas);
    gene_found = calloc(MAX_GENES + 100, sizeof(char));

    int global_id_count = 0;
    for(int i = 0; i < qtd_doencas; i++) {
        while (pos_buf < len_buf && buffer_arq[pos_buf] <= 32) pos_buf++;
        int p_ini = pos_buf;
        while (pos_buf < len_buf && buffer_arq[pos_buf] > 32) pos_buf++;
        int len_cod = pos_buf - p_ini;
        memcpy(lista_doencas[i].codigo, &buffer_arq[p_ini], len_cod);
        lista_doencas[i].codigo[len_cod] = '\0';
        
        lista_doencas[i].id_orig = i;
        lista_doencas[i].qtd_genes = fast_read_int();
        lista_doencas[i].gene_ids = malloc(sizeof(int) * lista_doencas[i].qtd_genes);
        
        for(int k = 0; k < lista_doencas[i].qtd_genes; k++) {
            while (pos_buf < len_buf && buffer_arq[pos_buf] <= 32) pos_buf++;
            int g_ini = pos_buf;
            while (pos_buf < len_buf && buffer_arq[pos_buf] > 32) pos_buf++;
            char original_char = buffer_arq[pos_buf];
            buffer_arq[pos_buf] = '\0';
            
            lista_doencas[i].gene_ids[k] = global_id_count;
            insert(&buffer_arq[g_ini], global_id_count++);
            
            buffer_arq[pos_buf] = original_char;
        }
    }

    build_ac();

    int u = 0;
    for (char *c = dna_ptr; *c; c++) {
        int idx = mapa_base[(unsigned char)*c];
        if (idx != -1) {
            u = nodes[u].next[idx];
            visited[u] = 1;
        }
    }

    for (int i = nodes_count - 1; i >= 0; i--) {
        int curr = q_bfs[i];
        if (visited[curr]) {
            visited[nodes[curr].fail] = 1;
            int p = nodes[curr].head_gene;
            while (p) {
                gene_found[pool[p].id_gene] = 1;
                p = pool[p].next;
            }
        }
    }

    for (int i = 0; i < qtd_doencas; i++) {
        int enc = 0;
        for (int k = 0; k < lista_doencas[i].qtd_genes; k++) {
            if (gene_found[lista_doencas[i].gene_ids[k]]) enc++;
        }
        if (lista_doencas[i].qtd_genes > 0)
            lista_doencas[i].percentual = (enc * 100 + lista_doencas[i].qtd_genes / 2) / lista_doencas[i].qtd_genes;
        else 
            lista_doencas[i].percentual = 0;
    }

    ordenar(lista_doencas, qtd_doencas);
    
    char *out_path = (argc > 2) ? argv[2] : "sequenciamento.output.txt";
    FILE *fout = fopen(out_path, "w");
    if (fout) {
        for (int i = 0; i < qtd_doencas; i++) {
            fprintf(fout, "%s->%d%%\n", lista_doencas[i].codigo, lista_doencas[i].percentual);
        }
        fclose(fout);
    }

    return 0;
}