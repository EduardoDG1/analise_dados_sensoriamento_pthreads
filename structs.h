#ifndef STRUCTS_H
#define STRUTCS_H

#define SIZE_DATAHORA_FULL 
#define SIZE_DATAHORA 20
#define SIZE_DATA 11
#define NUMBER_SPREADING_FACTORS 6
#define BUFFER_SIZE 100

#include <pthread.h>   
#include <semaphore.h> // sem_t, sem_wait, sem_post

#include  "cJSON.h"

#define LOG_QUEUE_SIZE 128
#define LOG_MSG_SIZE 256

typedef struct {
    char mensagem[LOG_MSG_SIZE];
    int ultimo;
} ItemLog;

typedef struct {
    ItemLog buffer[LOG_QUEUE_SIZE];
    int head, tail, count;
    pthread_mutex_t mutex;
    sem_t sem_empty;
    sem_t sem_full;
} LogQueue;

typedef struct {
    char *payloadStr;  // string do brute_data/payload, alocada pela thread leitora
    char payloadDate[SIZE_DATA];
    int ultimo;        
} ItemBuffer;

typedef struct {
    ItemBuffer buffer[BUFFER_SIZE];
    int head, tail, count;
    pthread_mutex_t mutex;
    sem_t sem_empty;
    sem_t sem_full;
    int total_registros;
    int registros_lidos;
    int registros_processados;
    int leitura_concluida;
    int registros_duplicados;
} SharedBuffer;


typedef struct structs
{
    int dia;
    int mes;
    int ano;
    int hora;
    int minuto;
    int segundo;
}DATAHORA;


typedef struct
{
    double maxima;
    char dataHoraMaxima[SIZE_DATAHORA];
    double minima;
    char dataHoraMinima[SIZE_DATAHORA];
    double media;
}DADOSTEMPERATURA;

typedef struct
{
    double maxima;
    char dataHoraMaxima[SIZE_DATAHORA];
    double minima;
    char dataHoraMinima[SIZE_DATAHORA];
    double media;
}DADOSUMIDADE;

typedef struct
{
    double maxima;
    char dataHoraMaxima[SIZE_DATAHORA];
    double minima;
    char dataHoraMinima[SIZE_DATAHORA];
    double media;
}DADOSPRESSAOTMOSFERICA;

typedef struct
{
    double inicial;
    double final;
    double consumo;
}DADOSBATERIA;

typedef struct
{
    int spreadingFactors[NUMBER_SPREADING_FACTORS];
    int nItens;
}DADOSSPREADINGFACTOR;

typedef struct
{
    int temperatura;
    int umidade;
    int pressaoAtmosferica;
}CONTADORES;

typedef struct
{
    int numeroRegistros;
    DADOSTEMPERATURA dadosTemperatura;
    DADOSUMIDADE dadosUmidade;
    DADOSPRESSAOTMOSFERICA dadosPressaoAtmosferica;
    DADOSBATERIA dadosBateria;
    DADOSSPREADINGFACTOR dadosSpreadingFactors;
    CONTADORES contadores;
}ESTATISTICASCAXIAS;

typedef struct
{
    int numeroRegistros;
    DADOSTEMPERATURA dadosTemperatura;
    DADOSUMIDADE dadosUmidade;
    DADOSPRESSAOTMOSFERICA dadosPressaoAtmosferica;
    DADOSBATERIA dadosBateria;
    DADOSSPREADINGFACTOR dadosSpreadingFactors;
    CONTADORES contadores; 
}ESTATISTICASBENTO;

typedef struct
{
    SharedBuffer *shared;
    LogQueue *lq;
    ESTATISTICASCAXIAS estatisticasCaxias;
    ESTATISTICASBENTO estatisticasBento;
    char periodoInicio[SIZE_DATA];
    char periodoFim[SIZE_DATA];
}ARGSPROCESSARJSONMQTT;

typedef struct
{
    SharedBuffer *shared;
    LogQueue *lq;
    ESTATISTICASCAXIAS estatisticasCaxias;
    ESTATISTICASBENTO estatisticasBento;
    char periodoInicio[SIZE_DATA];
    char periodoFim[SIZE_DATA];
}ARGSPROCESSARJSON;

typedef struct
{
    char *nomeArquivo;
    cJSON *json;
    SharedBuffer *shared;
    LogQueue *lq;
}ARGSCARREGARJSON;

typedef struct
{
    char *arquivo1;
    int nItensArquivo1;
    int ignoradosArquivo1;
    DATAHORA periodoInicioArquivo1;
    DATAHORA periodoFimArquivo1;
    char*arquivo2;
    int nItensArquivo2;
    int ignoradosArquivo2;
    DATAHORA periodoInicioArquivo2;
    DATAHORA periodoFimArquivo2;
    float tempo;
    ESTATISTICASCAXIAS estatisticasCaxias;
    ESTATISTICASBENTO estatisticasBento;
    LogQueue *lq;
}ARGSIMPRIMIRDADOS;

typedef struct {
    char timestamp[30];
    int payloadId;
} ItemDeduplicacao;
#endif