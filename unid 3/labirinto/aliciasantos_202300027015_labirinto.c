#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX 100
#define BUF_OUT_SIZE 1000000

// Matrizes globais para acesso ultra rápido e economia de pilha
// Se colocar isso dentro da função, a pilha explode e o PC chora.
char mapa[MAX][MAX];
char visitado[MAX][MAX];
char out_buffer[BUF_OUT_SIZE];
int out_ptr = 0;

typedef struct {
    int l, c;
} Pos;

// Prioridade confirmada: D, F, E, T
// Ordem de movimento pra não se perder nesse labirinto do inferno
const int dl[] = {0, -1, 0, 1};
const int dc[] = {1, 0, -1, 0};
const char* labels[] = {"D->", "F->", "E->", "T->"};

// Função ultra rápida para escrever inteiros no buffer sem sprintf
// Porque o sprintf é lento pra desgraça e a gente tem pressa
void fast_int(int n) {
    if (n == 0) {
        out_buffer[out_ptr++] = '0';
        return;
    }
    char temp[10];
    int i = 0;
    while (n > 0) {
        temp[i++] = (n % 10) + '0';
        n /= 10;
    }
    // Desempilha essa bosta de trás pra frente
    while (i > 0) out_buffer[out_ptr++] = temp[--i];
}

// Função rápida para strings fixas
void fast_str(const char* s) {
    while (*s) out_buffer[out_ptr++] = *s++;
}

void resolver(int id, FILE* in, FILE* out) {
    int largura, altura;
    if (fscanf(in, "%d %d", &largura, &altura) != 2) return;

    Pos inicio, atual;
    for (int i = 0; i < altura; i++) {
        for (int j = 0; j < largura; j++) {
            char c;
            // Pula espaços, \n e \r rapidamente. Ô parser chato de fazer.
            do { c = fgetc(in); } while (c <= ' '); 
            mapa[i][j] = c;
            visitado[i][j] = 0;
            if (c == 'X') inicio = (Pos){i, j};
        }
    }

    out_ptr = 0;
    fast_str("L"); fast_int(id); fast_str(":INI@");
    fast_int(inicio.l); fast_str(","); fast_int(inicio.c); fast_str("|");
    
    visitado[inicio.l][inicio.c] = 1;
    Pos pilha[MAX * MAX]; // Pilha manual pra não usar recursão e foder tudo
    int topo = 0;
    pilha[topo++] = inicio;

    while (topo > 0) {
        atual = pilha[topo - 1];

        // Verificação de borda (saída)
        // Se bater na borda e não for o início, achamos a saída dessa porcaria
        if ((atual.l != inicio.l || atual.c != inicio.c) &&
            (atual.l == 0 || atual.l == altura - 1 || atual.c == 0 || atual.c == largura - 1)) {
            fast_str("FIM@"); fast_int(atual.l); fast_str(","); fast_int(atual.c); fast_str("\n");
            fwrite(out_buffer, 1, out_ptr, out);
            return;
        }

        int moveu = 0;
        // Tenta mover para as 4 direções (D, F, E, T)
        for (int i = 0; i < 4; i++) {
            int nl = atual.l + dl[i];
            int nc = atual.c + dc[i];

            // Verifica se está dentro do mapa e se não é parede ou se já não estivemos lá
            if (nl >= 0 && nl < altura && nc >= 0 && nc < largura &&
                mapa[nl][nc] == '0' && !visitado[nl][nc]) {
                
                visitado[nl][nc] = 1;
                fast_str(labels[i]); fast_int(nl); fast_str(","); fast_int(nc); fast_str("|");
                pilha[topo++] = (Pos){nl, nc};
                moveu = 1;
                break;
            }
        }

        // Se não moveu, é porque bateu num beco sem saída. Backtrack nessa merda.
        if (!moveu) {
            Pos saindo = pilha[--topo];
            if (topo > 0) {
                Pos volta = pilha[topo - 1];
                fast_str("BT@"); fast_int(saindo.l); fast_str(","); fast_int(saindo.c);
                fast_str("->"); fast_int(volta.l); fast_str(","); fast_int(volta.c); fast_str("|");
            }
        }
    }

    // Se saiu do while sem achar a borda, o labirinto é impossível. Puta que pariu.
    fast_str("FIM@-,-\n");
    fwrite(out_buffer, 1, out_ptr, out);
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 1; // Cadê os argumentos, animal?
    FILE* in = fopen(argv[1], "r");
    FILE* out = fopen(argv[2], "w");
    if (!in || !out) return 1;

    int n;
    // Lê quantos labirintos temos que resolver hoje
    if (fscanf(in, "%d", &n) == 1) {
        for (int i = 0; i < n; i++) resolver(i, in, out);
    }

    fclose(in);
    fclose(out);
    return 0;
}
