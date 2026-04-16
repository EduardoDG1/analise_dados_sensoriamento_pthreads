#ifndef STRUCTS_H
#define STRUTCS_H

#define SIZE_DATAHORA 20
#define SIZE_DATA 11
#define NUMBER_SPREADING_FACTORS 6

#include  "cJSON.h"

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


#endif