EXPERIMENTO QUICKSORT - TRES NIVEIS DE BITWISE
==============================================

OBJETIVO E GARANTIA METODOLOGICA
--------------------------------
Os tres niveis continuam usando a mesma estrutura de Quicksort: particao de
Lomuto, pivo no elemento central, mesmos limites, fluxo de recursao e quantidade
de comparacoes. Somente as operacoes definidas para comparacao, troca e calculo
do ponto medio variam entre nenhum, moderado e alto.

A orquestracao, a coleta de metadados e a gravacao de evidencias foram separadas
do algoritmo. O arquivo quicksort.c e a implementacao medida; hardware.h coleta
informacoes da maquina; condicoes.h contem a instrumentacao de execucao;
executar_experimento.py coordena as passagens, mostra progresso e registra a
saida; Makefile oferece atalhos.

ARQUIVOS GERADOS
----------------
1) evidencias/teste_01_txt_resultados_detalhados.txt
   Relatorio textual da passagem TXT, com o cabecalho de hardware no topo,
   progresso por tamanho N e todas as saidas detalhadas dos processos dessa fase.

2) evidencias/teste_02_csv_dados_estruturados.csv
   Dados tabulares da passagem CSV. As linhas iniciais comecadas por "#" sao
   metadados de hardware. A linha de colunas CSV vem logo depois dos metadados.
   Para ler com pandas: pandas.read_csv("evidencias/teste_02_csv_dados_estruturados.csv",
   comment="#"). Em outros leitores, ignore as linhas iniciais que comecam por #.

As passagens TXT e CSV sao execucoes independentes: o CSV nao e uma conversao
ou copia do TXT; os tempos sao coletados novamente em uma segunda passagem.

3) evidencias/registro_integral_execucao.log
   Registro mestre da execucao: cabecalho de hardware, parametros, compilacao,
   mensagens de progresso, todas as saidas detalhadas das fases TXT, CSV e
   controle (inclusive as que nao aparecem no terminal),
   codigos de saida dos processos e encerramento.
   Este e o arquivo de evidencia completo da execucao orquestrada. O programa
   grava nele todo o texto de stdout/stderr produzido pelo proprio experimento.
   Nao inclui comandos digitados no shell nem mensagens externas do terminal.

FEEDBACK NO TERMINAL
--------------------
Durante TXT, CSV e controle aparece uma linha para cada tamanho N, por exemplo:
[progresso] CSV - DADOS ESTRUTURADOS | teste 4/15 | N=5000 | 10 repeticao(oes)
por nivel (3 niveis)

Nas fases TXT e CSV, o terminal mostra somente o progresso por N para evitar
inundacao visual. As saidas detalhadas dos processos dessas duas fases sao
gravadas nos respectivos arquivos de resultado e no LOG mestre. Somente na fase
de controle os resultados detalhados sao exibidos no terminal; eles tambem sao
registrados integralmente no LOG mestre.

CABECALHO DE HARDWARE
---------------------
O cabecalho aparece uma unica vez no inicio da execucao orquestrada e no inicio
dos arquivos TXT e CSV. Inclui, conforme disponibilizado pelo sistema:
- fabricante e modelo do computador/notebook;
- modelo do processador e quantidade de processadores logicos;
- memoria RAM fisica total;
- caches L1, L2 e L3 (por instancia/cache reportado pelo sistema);
- sistema operacional, versao/build quando disponivel, arquitetura e hostname;
- data/hora da coleta e compilador C.

hardware.h usa interfaces do Linux e APIs/Registro do Windows. Quando o sistema
nao fornece determinado campo, ele aparece como "indisponivel"; nao e estimado.

COMO EXECUTAR - LINUX
---------------------
Dependencias: Python 3, GCC e GNU Make (para usar os atalhos make).
Em distribuicoes Debian/Ubuntu, em geral:
    sudo apt install build-essential python3

Na pasta do projeto:
    make experimento-completo

Tambem e possivel executar sem Make:
    python3 executar_experimento.py completo

COMO EXECUTAR - WINDOWS
-----------------------
Dependencias: Python 3 e GCC para Windows (recomendado MinGW-w64/MSYS2).
O codigo C usa APIs Win32 para coletar os metadados e o Registro do Windows;
por isso, o caminho suportado e GCC/MinGW-w64, nao o compilador MSVC.

No PowerShell, na pasta do projeto:
    py -3 executar_experimento.py completo

Se GNU Make estiver instalado e no PATH, tambem pode usar:
    make experimento-completo PYTHON="py -3"

O Makefile usa Python para orquestrar os testes, portanto nao depende de comandos
POSIX como rm, /dev/null ou loops do shell. O executavel gerado no Windows sera
quick.exe.

COMANDOS DISPONIVEIS
--------------------
    make experimento-completo   TXT, CSV e controle no terminal
    make analise-txt            somente TXT detalhado
    make analise-csv            somente CSV estruturado
    make terminal-controle      somente controle visual no terminal
    make quick-run N=100000     controle para um unico N
    make compilar               somente compila o executavel
    make clean                  remove executavel e arquivos de saida padrao

Sem Make:
    python3 executar_experimento.py completo
    python3 executar_experimento.py txt
    python3 executar_experimento.py csv
    python3 executar_experimento.py terminal
    python3 executar_experimento.py compilar

PARAMETROS
----------
Exemplo com tamanhos reduzidos para uma execucao manual:
    python3 executar_experimento.py completo --tamanhos "100 500 1000" --reps-analise 3

Parametros do Makefile:
    TAMANHOS="100 500 1000"
    REPS_ANALISE=10
    REPS_TERMINAL=1
    PCT=100
    ORD=r ou f
    ENT=v ou f
    ARQ_TXT=nome_do_relatorio.txt
    ARQ_CSV=nome_dos_dados.csv
    ARQ_LOG=nome_da_evidencia.log

PCT controla a porcentagem de chaves sorteadas em toda a faixa int32; os demais
valores ficam entre -32768 e 32767. ORD=r alterna a ordem dos niveis a cada
repeticao. ENT=v varia a entrada por repeticao, mas os tres niveis recebem o
mesmo vetor dentro de cada repeticao.

NOTAS DE PORTABILIDADE E MEDICAO
--------------------------------
A coleta de processador, RAM, caches, modelo e sistema operacional tem caminhos
especificos para Linux e Windows. Algumas metricas de condicoes.h dependem do
sistema: frequencia instantanea, RSS/page faults e controle de huge pages podem
aparecer como n/d ou aviso no Windows, pois nao possuem a mesma interface usada
no Linux. A ausencia de uma metrica e registrada, nao substituida por um valor
inventado. O comportamento e a instrumentacao nao devem ser interpretados como
controle identico de hardware entre sistemas operacionais.

Os tamanhos padrao chegam a 10.000.000 elementos; esse vetor exige memoria
substancial. Ajuste TAMANHOS conforme a RAM disponivel.
