/*
 * Solução Final Corrigida - Transportadora Poxim Tech
 * Ajuste: Ordem de impressão dos itens (preservar ordem do backtracking)
 * para corresponder exatamente ao arquivo saida.txt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Definição das estruturas
typedef struct {
    char codigo[20];
    double valor;
    int peso;
    int volume;
    int carregado; // 0 = não, 1 = sim
} Item;

typedef struct {
    char placa[10];
    int cap_peso;
    int cap_volume;
} Veiculo;

// Função auxiliar para retornar o maior valor
double max_val(double a, double b) {
    return (a > b) ? a : b;
}

// Função para liberar a memória da matriz 3D
void liberar_matriz(double ***matriz, int n, int w) {
    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= w; j++) {
            free(matriz[i][j]);
        }
        free(matriz[i]);
    }
    free(matriz);
}

// Função principal de otimização (Algoritmo da Mochila 3D)
void processar_carga(Veiculo caminhao, Item *itens, int qtd_itens, FILE *saida) {
    int W = caminhao.cap_peso;
    int V = caminhao.cap_volume;
    
    // Mapear apenas itens ainda não carregados
    int itens_disponiveis = 0;
    int *indices_map = (int *)malloc(qtd_itens * sizeof(int)); 
    
    for (int i = 0; i < qtd_itens; i++) {
        if (!itens[i].carregado) {
            indices_map[itens_disponiveis] = i;
            itens_disponiveis++;
        }
    }

    int n = itens_disponiveis;

    // 1. Alocação Dinâmica DP (n+1) x (W+1) x (V+1)
    double ***dp = (double ***)malloc((n + 1) * sizeof(double **));
    for (int i = 0; i <= n; i++) {
        dp[i] = (double **)malloc((W + 1) * sizeof(double *));
        for (int j = 0; j <= W; j++) {
            dp[i][j] = (double *)calloc((V + 1), sizeof(double));
        }
    }

    // 2. Preenchimento da Tabela (Programação Dinâmica)
    for (int i = 1; i <= n; i++) {
        int idx_real = indices_map[i - 1];
        int peso_item = itens[idx_real].peso;
        int vol_item = itens[idx_real].volume;
        double valor_item = itens[idx_real].valor;

        for (int w = 0; w <= W; w++) {
            for (int v = 0; v <= V; v++) {
                if (peso_item > w || vol_item > v) {
                    dp[i][w][v] = dp[i - 1][w][v];
                } else {
                    double nao_levar = dp[i - 1][w][v];
                    double levar = dp[i - 1][w - peso_item][v - vol_item] + valor_item;
                    dp[i][w][v] = max_val(nao_levar, levar);
                }
            }
        }
    }

    // 3. Backtracking (Recuperar quais itens foram escolhidos)
    // Armazena na ordem inversa (do último item do input para o primeiro)
    int *itens_escolhidos_idx = (int *)malloc(n * sizeof(int));
    int qtd_escolhidos = 0;
    
    double valor_total = 0;
    int peso_total = 0;
    int vol_total = 0;
    
    int w_atual = W;
    int v_atual = V;

    for (int i = n; i > 0; i--) {
        if (dp[i][w_atual][v_atual] != dp[i - 1][w_atual][v_atual]) {
            int idx_real = indices_map[i - 1];
            
            itens[idx_real].carregado = 1; // Marca como usado
            itens_escolhidos_idx[qtd_escolhidos++] = idx_real;
            
            valor_total += itens[idx_real].valor;
            peso_total += itens[idx_real].peso;
            vol_total += itens[idx_real].volume;
            
            w_atual -= itens[idx_real].peso;
            v_atual -= itens[idx_real].volume;
        }
    }

    // 4. Escrita no arquivo de saída
    fprintf(saida, "[%s]R$%.2f,", caminhao.placa, valor_total);
    
    int perc_peso = (caminhao.cap_peso > 0) ? (int)round(((double)peso_total / caminhao.cap_peso) * 100) : 0;
    int perc_vol = (caminhao.cap_volume > 0) ? (int)round(((double)vol_total / caminhao.cap_volume) * 100) : 0;
    
    fprintf(saida, "%dKG(%d%%),%dL(%d%%)->", peso_total, perc_peso, vol_total, perc_vol);

    // --- CORREÇÃO: Ordem de Impressão ---
    // O arquivo saida.txt lista os itens na ordem do backtracking (Decrescente de índice original).
    // Antes estava invertendo para (Crescente), o que gerava a diferença.
    for (int i = 0; i < qtd_escolhidos; i++) {
        fprintf(saida, "%s", itens[itens_escolhidos_idx[i]].codigo);
        if (i < qtd_escolhidos - 1) {
            fprintf(saida, ",");
        }
    }
    fprintf(saida, "\n");

    // 5. Limpeza de memória local
    liberar_matriz(dp, n, W);
    free(indices_map);
    free(itens_escolhidos_idx);
}

int main(int argc, char *argv[]) {
    // Validação de argumentos para evitar erro se não passar os arquivos
    if (argc != 3) {
        printf("Uso: %s <entrada> <saida>\n", argv[0]);
        return 1;
    }

    FILE *entrada = fopen(argv[1], "r");
    if (!entrada) {
        printf("Erro ao abrir arquivo de entrada: %s\n", argv[1]);
        return 1;
    }

    int qtd_veiculos;
    if (fscanf(entrada, "%d", &qtd_veiculos) != 1) return 1;

    Veiculo *frota = (Veiculo *)malloc(qtd_veiculos * sizeof(Veiculo));
    for (int i = 0; i < qtd_veiculos; i++) {
        fscanf(entrada, "%s %d %d", frota[i].placa, &frota[i].cap_peso, &frota[i].cap_volume);
    }

    int qtd_itens;
    if (fscanf(entrada, "%d", &qtd_itens) != 1) return 1;

    Item *itens = (Item *)malloc(qtd_itens * sizeof(Item));
    for (int i = 0; i < qtd_itens; i++) {
        fscanf(entrada, "%s %lf %d %d", itens[i].codigo, &itens[i].valor, &itens[i].peso, &itens[i].volume);
        itens[i].carregado = 0;
    }
    fclose(entrada);

    FILE *saida = fopen(argv[2], "w");
    if (!saida) {
        printf("Erro ao criar arquivo de saida: %s\n", argv[2]);
        free(frota);
        free(itens);
        return 1;
    }

    // Processar Veículos
    for (int i = 0; i < qtd_veiculos; i++) {
        processar_carga(frota[i], itens, qtd_itens, saida);
    }

    // Processar Pendentes
    double valor_pendente = 0;
    int peso_pendente = 0;
    int vol_pendente = 0;
    
    for (int i = 0; i < qtd_itens; i++) {
        if (!itens[i].carregado) {
            valor_pendente += itens[i].valor;
            peso_pendente += itens[i].peso;
            vol_pendente += itens[i].volume;
        }
    }

    fprintf(saida, "PENDENTE:R$%.2f,%dKG,%dL->", valor_pendente, peso_pendente, vol_pendente);
    
    int primeiro = 1;
    for (int i = 0; i < qtd_itens; i++) {
        if (!itens[i].carregado) {
            if (!primeiro) {
                fprintf(saida, ",");
            }
            fprintf(saida, "%s", itens[i].codigo);
            primeiro = 0;
        }
    }
    fprintf(saida, "\n");

    fclose(saida);
    free(frota);
    free(itens);
    
    printf("Processamento concluido com sucesso.\n");

    return 0;
}