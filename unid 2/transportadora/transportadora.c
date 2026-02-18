#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_W 101
#define MAX_V 101

typedef struct {
    char codigo[20];
    float valor;
    int peso, volume, disponivel;
} Item;

typedef struct {
    char placa[10];
    int cap_peso, cap_volume;
} Veiculo;

// Alocação dinâmica global para evitar o erro de 'relocation' em memória estática
float ***dp;

void inicializar_matriz(int n_i) {
    dp = (float ***)malloc((n_i + 1) * sizeof(float **));
    for (int i = 0; i <= n_i; i++) {
        dp[i] = (float **)malloc(MAX_W * sizeof(float *));
        for (int j = 0; j < MAX_W; j++) {
            dp[i][j] = (float *)calloc(MAX_V, sizeof(float));
        }
    }
}

void processar_veiculo(Veiculo v, Item *itens, int n_i, FILE *out) {
    int W = v.cap_peso;
    int V = v.cap_volume;

    // Preenchimento da Tabela DP 3D
    for (int i = 1; i <= n_i; i++) {
        int pi = itens[i - 1].peso;
        int vi = itens[i - 1].volume;
        float vali = itens[i - 1].valor;
        int disp = itens[i - 1].disponivel;

        for (int w = 0; w <= W; w++) {
            for (int vol = 0; vol <= V; vol++) {
                if (!disp || pi > w || vi > vol) {
                    dp[i][w][vol] = dp[i - 1][w][vol];
                } else {
                    float v_com = dp[i - 1][w - pi][vol - vi] + vali;
                    float v_sem = dp[i - 1][w][vol];
                    // O >= garante a prioridade de itens posteriores no arquivo
                    dp[i][w][vol] = (v_com >= v_sem) ? v_com : v_sem;
                }
            }
        }
    }

    // Backtracking para identificar os itens
    int w_at = W, v_at = V;
    int sel[5001], n_sel = 0;
    int p_tot = 0, vol_tot = 0;

    for (int i = n_i; i > 0; i--) {
        // Se o valor na camada i é diferente da camada i-1, o item foi incluído
        if (itens[i - 1].disponivel && dp[i][w_at][v_at] != dp[i - 1][w_at][v_at]) {
            itens[i - 1].disponivel = 0;
            sel[n_sel++] = i - 1;
            p_tot += itens[i - 1].peso;
            vol_tot += itens[i - 1].volume;
            w_at -= itens[i - 1].peso;
            v_at -= itens[i - 1].volume;
        }
    }

    // Saída Formatada: Truncamento de % e IMPRESSÃO REVERSA DA LISTA
    int pp = (W > 0) ? (p_tot * 100) / W : 0;
    int vp = (V > 0) ? (vol_tot * 100) / V : 0;

    fprintf(out, "[%s]R$%.2f,%dKG(%d%%),%dL(%d%%)->", v.placa, (double)dp[n_i][W][V], p_tot, pp, vol_tot, vp);
    
    // Invertemos a ordem do backtracking para bater com a ordem original do professor
    for (int i = n_sel - 1; i >= 0; i--) {
        fprintf(out, "%s%s", itens[sel[i]].codigo, (i == 0) ? "" : ",");
    }
    fprintf(out, "\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;

    FILE *in = fopen(argv[1], "r");
    FILE *out = fopen(argv[2], "w");
    if (!in || !out) return 1;

    int n_v, n_i;
    if (fscanf(in, "%d", &n_v) != 1) return 1;
    Veiculo *frota = malloc(n_v * sizeof(Veiculo));
    for (int i = 0; i < n_v; i++) 
        if (fscanf(in, "%s %d %d", frota[i].placa, &frota[i].cap_peso, &frota[i].cap_volume) != 3) break;

    if (fscanf(in, "%d", &n_i) != 1) return 1;
    Item *itens = malloc(n_i * sizeof(Item));
    for (int i = 0; i < n_i; i++) {
        if (fscanf(in, "%s %f %d %d", itens[i].codigo, &itens[i].valor, &itens[i].peso, &itens[i].volume) != 4) break;
        itens[i].disponivel = 1;
    }

    inicializar_matriz(n_i);

    for (int i = 0; i < n_v; i++) {
        processar_veiculo(frota[i], itens, n_i, out);
    }

    // Relatório de Pendentes
    float val_p = 0; int pes_p = 0, vol_p = 0;
    for (int i = 0; i < n_i; i++) {
        if (itens[i].disponivel) {
            val_p += itens[i].valor; pes_p += itens[i].peso; vol_p += itens[i].volume;
        }
    }
    fprintf(out, "PENDENTE:R$%.2f,%dKG,%dL->", (double)val_p, pes_p, vol_p);
    int first = 1;
    for (int i = 0; i < n_i; i++) {
        if (itens[i].disponivel) {
            fprintf(out, "%s%s", first ? "" : ",", itens[i].codigo);
            first = 0;
        }
    }
    fprintf(out, "\n");

    fclose(in); fclose(out);
    return 0;
}