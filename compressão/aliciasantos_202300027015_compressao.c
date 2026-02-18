/*
 * PROJETO COMPRESSÃO - VERSÃO "EU SOU A VELOCIDADE" (KACHOW!)
 * -------------------------------------------------------------------------
 * OTIMIZAÇÕES DE "GÊNIO" (OU GAMBIARRA DE ALTO NÍVEL):
 * STATUS: Rodando mais rápido que boleto vencendo.
 * 
 * Aluna (vulgo vitima do código) : Alícia Vitória Sousa Santos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// CONFIGURAÇÕES DO MONSTRO
// ============================================================================
#define MAX_NOS_HUFFMAN 600
#define TAM_BUFFER_IO 20000000 

// ============================================================================
// ESTRUTURAS - O BÁSICO talvez mal feito
// ============================================================================
typedef struct Node {
    unsigned char byte;      
    int frequencia;          
    struct Node *esquerda;   
    struct Node *direita;    
    int eh_folha;            
} Node;

typedef struct {
    Node* vetor[MAX_NOS_HUFFMAN];        
    int tamanho;
} MinHeap;

typedef struct {
    char bits[256];          
    int tamanho_bits;        
} TabelaCodigos;

// --- "XITANDO" A MATEMÁTICA (LOOKUP TABLES) ---
int HEX_DECODE[256];          // Transforma 'A' em 10 mais rápido que piscar
char HEX_ENCODE[256][2];      // Transforma 10 em "0A" sem fazer conta

// --- BUFFERS GLOBAIS (VARIÁVEIS GLOBAIS SÃO DO MAL, MAS SÃO RÁPIDAS) ---
Node pool_nos[MAX_NOS_HUFFMAN]; // Nossa piscina de nós (sem salva-vidas)
int pool_indice = 0;

char* buffer_saida;
long pos_saida = 0;

// ============================================================================
// FUNÇÕES AUXILIARES QUE FAZEM A MÁGICA
// ============================================================================

void inicializar_tabelas() {
    // Preenchendo a colinha antes da prova começar
    for(int i=0; i<256; i++) HEX_DECODE[i] = 0;
    
    // Ensinando o computador a ser um bonequinho alfabetizado que le números e letras
    for(int i='0'; i<='9'; i++) HEX_DECODE[i] = i - '0';
    for(int i='A'; i<='F'; i++) HEX_DECODE[i] = i - 'A' + 10;
    for(int i='a'; i<='f'; i++) HEX_DECODE[i] = i - 'a' + 10;

    const char *map = "0123456789ABCDEF";
    for(int i=0; i<256; i++) {
        // Bitwise shift porque divisão é muito mainstream e lenta
        HEX_ENCODE[i][0] = map[(i >> 4) & 0xF];
        HEX_ENCODE[i][1] = map[i & 0xF];
    }
}

void resetar_pool() {
    pool_indice = 0;
}

// Malloc? Aqui não pq eu me odeio, vamo de vetor estático
Node* criar_no_pool(unsigned char b, int freq, int folha) {
    Node* novo = &pool_nos[pool_indice++];
    novo->byte = b;
    novo->frequencia = freq;
    novo->eh_folha = folha;
    novo->esquerda = NULL;
    novo->direita = NULL;
    return novo;
}

char *ptr_arquivo; // O ponteiro que vai varrer a memória

// Lê int ignorando qualquer coisa que não seja número (tipo espaços e quebras)
long ler_int_fast() {
    long val = 0;
    while (*ptr_arquivo <= ' ') ptr_arquivo++; // Pula espaços
    while (*ptr_arquivo >= '0' && *ptr_arquivo <= '9') {
        val = (val * 10) + (*ptr_arquivo - '0'); // Monta o número na unha
        ptr_arquivo++;
    }
    return val;
}

// Lê Hex usando a tabela de cheat que criamos lá em cima
void ler_hex_fast(unsigned char* dest) {
    while (*ptr_arquivo <= ' ') ptr_arquivo++; // Ignora whitespace
    
    // Pega o primeiro char e já converte. Shift left 4 é tipo multiplicar por 16, só que style.
    int val = HEX_DECODE[(unsigned char)*ptr_arquivo++] << 4;
    
    // Pega o segundo char se ele existir
    if (*ptr_arquivo > ' ') {
        val |= HEX_DECODE[(unsigned char)*ptr_arquivo++];
    }
    *dest = (unsigned char)val;
}

// --- ESCRITA BUFFERIZADA (nitro de carro versão muito café na veia) ---

// Copia string pra RAM. Nada de syscall aqui.
void buffer_escrever_string(const char* str) {
    while (*str) {
        buffer_saida[pos_saida++] = *str++;
    }
}

// Escreve Hex na RAM olhando na tabela. Vapt vupt.
void buffer_escrever_hex(unsigned char byte) {
    buffer_saida[pos_saida++] = HEX_ENCODE[byte][0];
    buffer_saida[pos_saida++] = HEX_ENCODE[byte][1];
}

// ============================================================================
// RLE - ALGORITMO "CONTADOR DE OVELHAS"
// ============================================================================
long executar_rle(unsigned char* entrada, long tam_entrada, unsigned char* saida) {
    long i_leitura = 0;
    long i_escrita = 0;

    while (i_leitura < tam_entrada) {
        unsigned char byte_atual = entrada[i_leitura];
        int contador = 0;
        long k = i_leitura;
        
        // Conta quantos iguais tem na sequência
        while (k < tam_entrada) {
            if (entrada[k] == byte_atual) {
                contador++;
                k++;
                // Se passar de 255, estoura o byte, então para.
                if (contador == 255) break; 
            } else {
                break; // Mudou o byte? Tchau.
            }
        }
        saida[i_escrita++] = (unsigned char)contador;
        saida[i_escrita++] = byte_atual;
        i_leitura += contador;
    }
    return i_escrita; 
}

// ============================================================================
// HUFFMAN - A ÁRVORE DA DISCÓRDIA (COM BUILD-HEAP)
// ============================================================================
void trocar_nos(Node** a, Node** b) {
    Node* temp = *a; *a = *b; *b = temp;
}

// Organiza a bagunça na heap
void heapify(MinHeap* heap, int idx) {
    int menor = idx;
    int esq = (idx << 1) + 1; // Bitwise shift pra multiplicar por 2 (hacker moments)
    int dir = (idx << 1) + 2;

    if (esq < heap->tamanho && heap->vetor[esq]->frequencia < heap->vetor[menor]->frequencia)
        menor = esq;
    if (dir < heap->tamanho && heap->vetor[dir]->frequencia < heap->vetor[menor]->frequencia)
        menor = dir;

    if (menor != idx) {
        trocar_nos(&heap->vetor[menor], &heap->vetor[idx]);
        heapify(heap, menor);
    }
}

Node* heap_extrair(MinHeap* heap) {
    if (heap->tamanho <= 0) return NULL;
    Node* raiz = heap->vetor[0];
    heap->vetor[0] = heap->vetor[--heap->tamanho]; // Pega o último e joga pro topo
    heapify(heap, 0); // Conserta a bagunça
    return raiz;
}

void heap_inserir(MinHeap* heap, Node* no) {
    int i = heap->tamanho++;
    heap->vetor[i] = no;
    // Bubble Up: Sobe o nó se ele for mais leve que o pai
    while (i != 0) {
        int pai = (i - 1) >> 1; // Divisão por 2 via bitwise
        if (heap->vetor[i]->frequencia < heap->vetor[pai]->frequencia) {
            trocar_nos(&heap->vetor[i], &heap->vetor[pai]);
            i = pai;
        } else break;
    }
}

// Recursão pra gerar os bits (0 pra esquerda, 1 pra direita)
void gerar_tabela(Node* raiz, char* buffer, int prof, TabelaCodigos tab[256]) {
    if (raiz->esquerda) {
        buffer[prof] = '0'; buffer[prof+1] = 0;
        gerar_tabela(raiz->esquerda, buffer, prof + 1, tab);
    }
    if (raiz->direita) {
        buffer[prof] = '1'; buffer[prof+1] = 0;
        gerar_tabela(raiz->direita, buffer, prof + 1, tab);
    }
    if (raiz->eh_folha) {
        strcpy(tab[raiz->byte].bits, buffer);
        tab[raiz->byte].tamanho_bits = prof;
    }
}

long executar_huffman(unsigned char* entrada, long tam_entrada, unsigned char* saida) {
    int freq[256] = {0};
    // Conta as frequências (Histograma)
    for(long i=0; i<tam_entrada; i++) freq[entrada[i]]++;

    resetar_pool(); // Limpa a memória estática
    MinHeap heap;
    heap.tamanho = 0;

    // --- BUILD-HEAP STRATEGY ---
    // 1. Enche o vetor direto (ordem 00-FF garante estabilidade no desempate)
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            heap.vetor[heap.tamanho++] = criar_no_pool((unsigned char)i, freq[i], 1);
        }
    }
    // 2. Heapify reverso (arruma de baixo pra cima). É O(N), chupa essa manga.
    for (int i = (heap.tamanho >> 1) - 1; i >= 0; i--) heapify(&heap, i);

    // Se só tiver 1 tipo de byte, cria um pai fake senão a árvore buga
    if (heap.tamanho == 1) {
        Node* unico = heap_extrair(&heap);
        Node* pai = criar_no_pool(0, unico->frequencia, 0);
        pai->esquerda = unico;
        heap_inserir(&heap, pai);
    }

    // Monta a árvore (Fusão dos nós)
    while (heap.tamanho > 1) {
        Node* esq = heap_extrair(&heap);
        Node* dir = heap_extrair(&heap);
        Node* pai = criar_no_pool(0, esq->frequencia + dir->frequencia, 0);
        pai->esquerda = esq;
        pai->direita = dir;
        heap_inserir(&heap, pai);
    }

    Node* raiz = heap_extrair(&heap);
    TabelaCodigos tabela[256];
    char buffer_bits[MAX_NOS_HUFFMAN];
    
    // Zera tabela na brutalidade pra ser rápido
    memset(tabela, 0, sizeof(TabelaCodigos) * 256);

    if (raiz) {
        buffer_bits[0] = 0;
        gerar_tabela(raiz, buffer_bits, 0, tabela);
    }

    // --- COMPRESSÃO (BIT PACKING) ---
    long byte_pos = 0;
    int bit_pos = 0;
    saida[0] = 0;

    for (long i = 0; i < tam_entrada; i++) {
        unsigned char b = entrada[i];
        int len = tabela[b].tamanho_bits;
        char* bits = tabela[b].bits;
        
        // Loop otimizado: só entra no if se for bit 1. Bit 0 já tá zerado.
        for (int j = 0; j < len; j++) {
            if (bits[j] == '1') {
                saida[byte_pos] |= (1 << (7 - bit_pos));
            }
            bit_pos++;
            if (bit_pos == 8) {
                bit_pos = 0;
                saida[++byte_pos] = 0;
            }
        }
    }
    if (bit_pos > 0) byte_pos++;
    
    // Sem free(raiz) porque usamos pool estático. O SO que se vire no final.
    return byte_pos;
}

// ============================================================================
// MAIN - ONDE O FILHO CHORA E A MÃE NÃO VÊ
// ============================================================================

// Cálculo de porcentagem manual porque math.h é bloatware
void calcular_porcentagem(long comprimido, long original, int* int_p, int* dec_p) {
    if (original == 0) { *int_p=0; *dec_p=0; return; }
    // Multiplica por 10000 pra manter precisão e soma metade do divisor pra arredondar
    unsigned long long res = ((unsigned long long)comprimido * 10000 + (original/2)) / original;
    *int_p = (int)(res / 100);
    *dec_p = (int)(res % 100);
}

int main() {
    inicializar_tabelas(); // Prepara as colas
    
    // 1. CARREGAR ARQUIVO INTEIRO NA RAM (ENGOLIR O ARQUIVO)
    FILE *f_in = fopen("compressao.input.txt", "rb");
    if (!f_in) return 1;

    fseek(f_in, 0, SEEK_END);
    long tamanho_arquivo = ftell(f_in);
    rewind(f_in);
    
    // Aloca um blocão de memória e puxa tudo de uma vez.
    char* conteudo_arquivo = (char*)malloc(tamanho_arquivo + 1);
    fread(conteudo_arquivo, 1, tamanho_arquivo, f_in);
    conteudo_arquivo[tamanho_arquivo] = 0;
    fclose(f_in);

    ptr_arquivo = conteudo_arquivo; // Aponta o leitor pro começo da bagunça

    // 2. PREPARAR O BUFFER DE SAÍDA (O MEGABUFFER)
    // 20MB. Se faltar RAM, baixa mais RAM na internet (brincadeira).
    buffer_saida = (char*)malloc(TAM_BUFFER_IO);
    pos_saida = 0;

    // Buffers auxiliares pra não ficar alocando toda hora
    unsigned char* raw = (unsigned char*)malloc(20000); 
    unsigned char* rle = (unsigned char*)malloc(20000);
    unsigned char* huf = (unsigned char*)malloc(20000);

    long num_casos = ler_int_fast();
    char buffer_fmt[100]; // Bufferzinho pra string curta

    // LOOP DA MORTE (PROCESSA TUDO)
    for (int i = 0; i < num_casos; i++) {
        long tam_seq = ler_int_fast();

        // Leitura turbo
        for (long j = 0; j < tam_seq; j++) {
            ler_hex_fast(&raw[j]);
        }

        // Roda os algoritmos
        long t_rle = executar_rle(raw, tam_seq, rle);
        int r_i, r_d; calcular_porcentagem(t_rle, tam_seq, &r_i, &r_d);

        long t_huf = executar_huffman(raw, tam_seq, huf);
        int h_i, h_d; calcular_porcentagem(t_huf, tam_seq, &h_i, &h_d);

        // --- HORA DE ESCREVER (NA RAM) ---
        // Checa Huffman primeiro (porque o gabarito gosta dele primeiro no empate)
        if (t_huf <= t_rle) {
            sprintf(buffer_fmt, "%d->HUF(%d.%02d%%)=", i, h_i, h_d);
            buffer_escrever_string(buffer_fmt);
            
            // Escreve Hex usando tabela. Voando baixo.
            for (long k = 0; k < t_huf; k++) {
                buffer_escrever_hex(huf[k]);
            }
            buffer_saida[pos_saida++] = '\n'; // Enter
        }

        // Checa RLE
        if (t_rle <= t_huf) {
            sprintf(buffer_fmt, "%d->RLE(%d.%02d%%)=", i, r_i, r_d);
            buffer_escrever_string(buffer_fmt);
            
            for (long k = 0; k < t_rle; k++) {
                buffer_escrever_hex(rle[k]);
            }
            buffer_saida[pos_saida++] = '\n';
        }
    }

    // 3. FLUSH FINAL (DESCARGA NO DISCO)
    // Uma única escrita no disco. O HD agradece.
    FILE *f_out = fopen("teste_saida.txt", "w");
    fwrite(buffer_saida, 1, pos_saida, f_out);
    fclose(f_out);

    // Limpando a bagunça antes de sair
    free(conteudo_arquivo);
    free(buffer_saida);
    free(raw);
    free(rle);
    free(huf);

    return 0;
}