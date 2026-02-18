#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX 100
// Buffer gigante para evitar chamadas de sistema (I/O) durante o percurso
#define BUF_SIZE 1000000 

typedef struct {
    int l, c;
} Posicao;

typedef struct {
    char matriz[MAX][MAX];
    char visitado[MAX][MAX];
    int altura, largura;
} Labirinto;

// NOVA ORDEM DE PRIORIDADE (Baseada no seu feedback):
// 1. Direita (D), 2. Frente (F), 3. Esquerda (E), 4. Trás (T)
const int dl[] = {0, -1, 0, 1}; 
const int dc[] = {1, 0, -1, 0};
const char *labels[] = {"D->", "F->", "E->", "T->"};

Labirinto lab;

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;

    FILE *entrada = fopen(argv[1], "r");
    FILE *saida = fopen(argv[2], "w");
    if (!entrada || !saida) return 1;

    int num_labirintos;
    if (fscanf(entrada, "%d", &num_labirintos) != 1) return 1;

    char *output_buffer = malloc(BUF_SIZE);

    for (int n = 0; n < num_labirintos; n++) {
        int largura, altura;
        if (fscanf(entrada, "%d %d", &largura, &altura) != 2) break;

        lab.largura = largura;
        lab.altura = altura;
        Posicao inicio, atual;

        for (int i = 0; i < altura; i++) {
            for (int j = 0; j < largura; j++) {
                char c;
                do { c = fgetc(entrada); } while (c == ' ' || c == '\n' || c == '\r');
                lab.matriz[i][j] = c;
                lab.visitado[i][j] = 0;
                if (c == 'X') inicio = (Posicao){i, j};
            }
        }

        int ptr = sprintf(output_buffer, "L%d:INI@%d,%d|", n, inicio.l, inicio.c);
        lab.visitado[inicio.l][inicio.c] = 1;

        Posicao pilha[MAX * MAX];
        int topo = 0;
        pilha[topo++] = inicio;

        while (topo > 0) {
            atual = pilha[topo - 1];

            // Saída encontrada (Borda e não é o início)
            if ((atual.l != inicio.l || atual.c != inicio.c) &&
                (atual.l == 0 || atual.l == altura - 1 || atual.c == 0 || atual.c == largura - 1)) {
                ptr += sprintf(output_buffer + ptr, "FIM@%d,%d\n", atual.l, atual.c);
                fwrite(output_buffer, 1, ptr, saida);
                goto proximo;
            }

            int moveu = 0;
            for (int i = 0; i < 4; i++) {
                int nl = atual.l + dl[i];
                int nc = atual.c + dc[i];

                if (nl >= 0 && nl < altura && nc >= 0 && nc < largura &&
                    lab.matriz[nl][nc] == '0' && !lab.visitado[nl][nc]) {
                    
                    lab.visitado[nl][nc] = 1;
                    ptr += sprintf(output_buffer + ptr, "%s%d,%d|", labels[i], nl, nc);
                    pilha[topo++] = (Posicao){nl, nc};
                    moveu = 1;
                    break;
                }
            }

            if (!moveu) {
                Posicao saindo = pilha[--topo];
                if (topo > 0) {
                    Posicao voltando = pilha[topo - 1];
                    ptr += sprintf(output_buffer + ptr, "BT@%d,%d->%d,%d|", 
                                   saindo.l, saindo.c, voltando.l, voltando.c);
                }
            }
            
            // Segurança contra estouro de buffer (opcional)
            if (ptr > BUF_SIZE - 1000) {
                fwrite(output_buffer, 1, ptr, saida);
                ptr = 0;
            }
        }
        ptr += sprintf(output_buffer + ptr, "FIM@-,-\n");
        fwrite(output_buffer, 1, ptr, saida);

proximo:;
    }

    free(output_buffer);
    fclose(entrada);
    fclose(saida);
    return 0;
}