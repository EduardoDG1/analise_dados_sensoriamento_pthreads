#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include "structs.h"
#include "processData.h"

//Implementar função de gera logs

int main(){
    clock_t tempo = clock();

    ARGSCARREGARJSON parametrosCarregarJson = {"senzemo_cx_bg.json"};
    ARGSCARREGARJSON parametrosCarregarJsonMqtt = {"mqtt_senzemo_cx_bg.json"};
    
    //Leitura dos arquivos (paralelizar)
    carregarJson((void *)&parametrosCarregarJson);
    carregarJson((void *)&parametrosCarregarJsonMqtt);

    DADOSTEMPERATURA dadosTemperatura;
    dadosTemperatura.media = 0;
    dadosTemperatura.maxima = INT_MIN;
    dadosTemperatura.minima = INT_MAX;

    DADOSUMIDADE dadosUmidade;
    dadosUmidade.media = 0;
    dadosUmidade.maxima = INT_MIN;
    dadosUmidade.minima = INT_MAX;

    DADOSPRESSAOTMOSFERICA dadosPressaoAtmosferica;
    dadosPressaoAtmosferica.media = 0;
    dadosPressaoAtmosferica.maxima = INT_MIN;
    dadosPressaoAtmosferica.minima = INT_MAX;

    DADOSBATERIA dadosBateria;
    dadosBateria.inicial = 0;
    dadosBateria.final = 10; 

    DADOSSPREADINGFACTOR dadosSpreadingFactors;
    dadosSpreadingFactors.nItens = 0;

    ESTATISTICASCAXIAS estatisticasCaxias;
    estatisticasCaxias.dadosTemperatura = dadosTemperatura;
    estatisticasCaxias.dadosUmidade = dadosUmidade;
    estatisticasCaxias.dadosPressaoAtmosferica = dadosPressaoAtmosferica;
    estatisticasCaxias.dadosBateria = dadosBateria;
    estatisticasCaxias.dadosSpreadingFactors = dadosSpreadingFactors;
    estatisticasCaxias.numeroRegistros = 0;

    ESTATISTICASBENTO estatisticasBento;
    estatisticasBento.dadosTemperatura = dadosTemperatura;
    estatisticasBento.dadosUmidade = dadosUmidade;
    estatisticasBento.dadosPressaoAtmosferica = dadosPressaoAtmosferica;
    estatisticasBento.dadosBateria = dadosBateria;
    estatisticasBento.dadosSpreadingFactors = dadosSpreadingFactors;
    estatisticasBento.numeroRegistros = 0;

    //Calcular estatisticas (paralelizar)
    ARGSPROCESSARJSON parametrosProcessarJson;
    parametrosProcessarJson.json = parametrosCarregarJson.json; 
    parametrosProcessarJson.estatisticasCaxias = estatisticasCaxias;
    parametrosProcessarJson.estatisticasBento = estatisticasBento;

    processarJson((void *)&parametrosProcessarJson);

    ARGSPROCESSARJSONMQTT parametrosProcessarJsonMqtt;
    parametrosProcessarJsonMqtt.json = parametrosCarregarJsonMqtt.json; 
    parametrosProcessarJsonMqtt.estatisticasCaxias = estatisticasCaxias;
    parametrosProcessarJsonMqtt.estatisticasBento = estatisticasBento;

    processarJsonMqtt((void*)&parametrosProcessarJsonMqtt);

    //Finalização de cálculo de estatisticas
    ESTATISTICASCAXIAS estatisticasCaxiasFinal = calcularEstatisticasCaxias(parametrosProcessarJson.estatisticasCaxias, parametrosProcessarJsonMqtt.estatisticasCaxias);
    ESTATISTICASBENTO estatisticasBentoFinal = calcularEstatisticasBento(parametrosProcessarJson.estatisticasBento, parametrosProcessarJsonMqtt.estatisticasBento);

    float segundos = (float)(clock()-tempo)/CLOCKS_PER_SEC;

    DATAHORA dataInicioArquivo1, dataFimArquivo1, dataInicioArquivo2,dataFimArquivo2;

    sscanf(parametrosProcessarJson.periodoInicio, "%d-%d-%d", &dataInicioArquivo1.ano, &dataInicioArquivo1.mes, &dataInicioArquivo1.dia);
    sscanf(parametrosProcessarJson.periodoFim, "%d-%d-%d", &dataFimArquivo1.ano, &dataFimArquivo1.mes, &dataFimArquivo1.dia);

    sscanf(parametrosProcessarJsonMqtt.periodoInicio, "%d-%d-%d", &dataInicioArquivo2.ano, &dataInicioArquivo2.mes, &dataInicioArquivo2.dia);
    sscanf(parametrosProcessarJsonMqtt.periodoFim, "%d-%d-%d", &dataFimArquivo2.ano, &dataFimArquivo2.mes, &dataFimArquivo2.dia);

    ARGSIMPRIMIRDADOS argsImprimirDados = {
        "senzemo_cx_bg.json", 
        parametrosProcessarJson.estatisticasCaxias.numeroRegistros+parametrosProcessarJson.estatisticasBento.numeroRegistros,
        dataInicioArquivo1,
        dataFimArquivo1,
        "mqtt_senzemo_cx_bg.json", 
        parametrosProcessarJsonMqtt.estatisticasCaxias.numeroRegistros+parametrosProcessarJsonMqtt.estatisticasBento.numeroRegistros,
        dataInicioArquivo2,
        dataFimArquivo2,
        segundos,
        estatisticasCaxiasFinal, 
        estatisticasBentoFinal
    };

    //Impressão dos dados na tela
    imprimirInformacoesNaTela(argsImprimirDados);

    return 0;
}
