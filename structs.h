#ifndef STRUCTS_H
#define STRUTCS_H

#define SIZE_DATAHORA 20
#define SIZE_DATA 11
#define NUMBER_SPREADING_FACTORS 6

#include  "cJSON.h"

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
    int numeroRegistros;
    DADOSTEMPERATURA dadosTemperatura;
    DADOSUMIDADE dadosUmidade;
    DADOSPRESSAOTMOSFERICA dadosPressaoAtmosferica;
    DADOSBATERIA dadosBateria;
    DADOSSPREADINGFACTOR dadosSpreadingFactors;
}ESTATISTICASCAXIAS;

typedef struct
{
    int numeroRegistros;
    DADOSTEMPERATURA dadosTemperatura;
    DADOSUMIDADE dadosUmidade;
    DADOSPRESSAOTMOSFERICA dadosPressaoAtmosferica;
    DADOSBATERIA dadosBateria;
    DADOSSPREADINGFACTOR dadosSpreadingFactors;
}ESTATISTICASBENTO;

typedef struct
{
    cJSON *json;
    ESTATISTICASCAXIAS estatisticasCaxias;
    ESTATISTICASBENTO estatisticasBento;
    char periodoInicio[SIZE_DATA];
    char periodoFim[SIZE_DATA];
}ARGSPROCESSARJSONMQTT;

typedef struct
{
    cJSON *json;
    ESTATISTICASCAXIAS estatisticasCaxias;
    ESTATISTICASBENTO estatisticasBento;
    char periodoInicio[SIZE_DATA];
    char periodoFim[SIZE_DATA];
}ARGSPROCESSARJSON;

typedef struct
{
    char *nomeArquivo;
    cJSON *json;
}ARGSCARREGARJSON;

typedef struct
{
    char *arquivo1;
    int nItensArquivo1;
    DATAHORA periodoInicioArquivo1;
    DATAHORA periodoFimArquivo1;
    char*arquivo2;
    int nItensArquivo2;
    DATAHORA periodoInicioArquivo2;
    DATAHORA periodoFimArquivo2;
    float tempo;
    ESTATISTICASCAXIAS estatisticasCaxias;
    ESTATISTICASBENTO estatisticasBento;
}ARGSIMPRIMIRDADOS;

#endif